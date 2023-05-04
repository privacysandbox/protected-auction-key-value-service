# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""TODO(pmeric): Write module docstring."""

load("@bazel_skylib//rules:run_binary.bzl", "run_binary")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load(
    "@rules_pkg//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
)
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")

def _filepath_for(component_name, outfile):
    return "{}/{}".format(component_name, outfile)

def data_targets(data_path, config_path = "", certificate_components = {}):
    """Generates zip archive and related targets for data and config files

    Arguments:
        data_path: relative path to the data subdirectory
        config_path: relative path to the config subdirectory (optional)
        certificate_components: dict of {component: domain} for which certificates
                                are generated (optional). if empty, certificates will
                                be generated for each config component
    """

    component_data_files = [
        (
            # final path element (directory)
            "{}".format(component_data_dir.rsplit("/", 1)[1]),
            # csv files
            [
                struct(
                    name = "delta_csv_{}".format(idx),
                    infile = ":{}".format(delta_file),
                    # basename excluding extension
                    outfile = delta_file.rsplit("/", 1)[1].rsplit(".")[0],
                )
                for idx, delta_file in enumerate(native.glob(["{}/DELTA*.csv".format(component_data_dir)]))
            ],
            # js files
            [
                struct(
                    name = "delta_js_{}".format(idx),
                    infile = ":{}".format(delta_file),
                    # basename excluding extension
                    outfile = delta_file.rsplit("/", 1)[1].rsplit(".")[0],
                )
                for idx, delta_file in enumerate(native.glob(["{}/DELTA*.js".format(component_data_dir)]))
            ],
        )
        for component_data_dir in native.glob(["{}/*".format(data_path)], exclude_directories = 0)
    ]
    component_config_files = []
    if config_path:
        component_config_files = [
            (
                # final path element (directory)
                component_config_dir.rsplit("/", 1)[1],
                [
                    struct(
                        name = "delta_csv_{}".format(idx),
                        infile = ":{}".format(delta_file),
                    )
                    for idx, delta_file in enumerate(native.glob(["{}/**/*".format(component_config_dir)]))
                ],
            )
            for component_config_dir in native.glob(["{}/*".format(config_path)], exclude_directories = 0)
        ]

    [
        run_binary(
            name = "{}-{}".format(component_name, file.name),
            srcs = [
                file.infile,
            ],
            outs = [_filepath_for(component_name, file.outfile)],
            args = [
                "format_data",
                "--input_file",
                "$(location {})".format(file.infile),
                "--output_file",
                "$(location {})".format(_filepath_for(component_name, file.outfile)),
            ],
            tool = "//tools/data_cli",
        )
        for component_name, csv_files, _ in component_data_files
        for file in csv_files
    ]

    [
        run_binary(
            name = "{}-{}".format(component_name, file.name),
            srcs = [
                file.infile,
            ],
            outs = [_filepath_for(component_name, file.outfile)],
            args = [
                "--udf_file_path",
                "$(location {})".format(file.infile),
                "--output_path",
                "$(location {})".format(_filepath_for(component_name, file.outfile)),
            ],
            tool = "//tools/udf/udf_generator:udf_delta_file_generator",
        )
        for component_name, _, js_files in component_data_files
        for file in js_files
    ]

    [
        pkg_files(
            name = "data_{}".format(component_name),
            srcs = [":{}".format(_filepath_for(component_name, f.outfile)) for f in csv_files + js_files],
            attributes = pkg_attributes(mode = "0555"),
            prefix = "/{}/data".format(component_name),
        )
        for component_name, csv_files, js_files in component_data_files
    ]

    [
        pkg_files(
            name = "config_{}".format(component_name),
            srcs = [f.infile for f in config_files],
            attributes = pkg_attributes(mode = "0555"),
            prefix = "/{}/etc".format(component_name),
        )
        for component_name, config_files in component_config_files
    ]

    # generate certs
    [
        generate_ssl_certs(component_name, domain)
        for component_name, domain in certificate_components.items()
    ]

    pkg_zip(
        name = "sut_data",
        srcs = [":data_{}".format(component_name) for component_name, _, _ in component_data_files] +
               [":config_{}".format(component_name) for component_name, _ in component_config_files] +
               [":certs_{}".format(component_name) for component_name, _ in certificate_components.items()],
    )

def generate_ssl_certs(name, domain):
    extfile = "{}-extfile".format(name)
    write_file(
        name = "{}-file".format(extfile),
        out = extfile,
        content = ["""
authorityKeyIdentifier = keyid,issuer
basicConstraints = CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names
[alt_names]
DNS.1 = {domain}
        """.format(domain = domain)],
    )
    subj = "/C=US/ST=WA/O=SCP/CN={}".format(domain)
    cert_files = {
        "private_key": "private.key",
        "root_ca_cert": "root_ca.cert",
        "signed_cert": "signed.cert",
    }
    native.genrule(
        name = "{}-ssl-certs".format(name),
        srcs = [
            ":{}".format(extfile),
        ],
        outs = cert_files.values(),
        cmd_bash = """
# Generate a root CA certificate
openssl genrsa -out root_ca.key 2048
openssl req -x509 -sha256 -new -nodes -days 3650 -key root_ca.key -out $(@D)/{root_ca_cert} -subj {subj} &>/dev/null

# Generate CSR
openssl genrsa -out $(@D)/{private_key} 2048
openssl req -new -key $(@D)/{private_key} -out private.cert -subj {subj} &>/dev/null

# Sign SSL cert
openssl x509 -req -in private.cert -CA $(@D)/{root_ca_cert} -CAkey root_ca.key -CAcreateserial -days 7305 -out $(@D)/{signed_cert} -extfile $(execpath :{extfile}) &>/dev/null
        """.format(
            subj = subj,
            extfile = extfile,
            **cert_files
        ),
        local = True,
        message = "generate a set of SSL certificates",
    )

    pkg_files(
        name = "certs_{}".format(name),
        srcs = cert_files.values(),
        prefix = "/{}/certs".format(name),
    )
