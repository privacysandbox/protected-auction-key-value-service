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

"""Macro for generating SSL certificates."""

load("@bazel_skylib//rules:write_file.bzl", "write_file")

_CERT_FILENAMES = [
    "private.key",
    "root_ca.cert",
    "signed.cert",
]

def generate_ssl_certificate(name, domain, test_tags = []):
    """Generates a set of files for a signed SSL certificate.

    The generated target files are:
        root_ca.cert
        private.key
        signed.cert

    Arguments:
        name (string): base name for underlying targets and name of generated filegroup
        domain (string): certificate domain
        test_tags (list[string]): optional list of test tags
    """

    cert_files = ["{}_{}".format(name, f) for f in _CERT_FILENAMES]
    cert_file_dict = {canonical_f.replace(".", "_"): target_f for canonical_f, target_f in zip(_CERT_FILENAMES, cert_files)}
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
    native.genrule(
        name = "{}-ssl-certs".format(name),
        srcs = [
            ":{}".format(extfile),
        ],
        outs = cert_files,
        cmd_bash = """
# Generate a root CA certificate
openssl genrsa -out {name}-root_ca.key 2048 &>/dev/null
openssl req -x509 -sha256 -new -nodes -days 3650 -key {name}-root_ca.key -out $(@D)/{root_ca_cert} -subj {subj} &>/dev/null

# Generate CSR
openssl genrsa -out $(@D)/{private_key} 2048 &>/dev/null
openssl req -new -key $(@D)/{private_key} -out {name}-private.cert -subj {subj} &>/dev/null

# Sign SSL cert
openssl x509 -req -in {name}-private.cert -CA $(@D)/{root_ca_cert} -CAkey {name}-root_ca.key -CAcreateserial -days 7305 -out $(@D)/{signed_cert} -extfile $(execpath :{extfile}) &>/dev/null
        """.format(
            name = name,
            subj = subj,
            extfile = extfile,
            **cert_file_dict
        ),
        local = True,
        message = "generate a set of SSL certificates",
    )
    native.filegroup(
        name = name,
        srcs = [":{}".format(f) for f in cert_files],
    )
    native.sh_test(
        name = "{}_test".format(name),
        data = [":{}_{}".format(name, f) for f in ("root_ca.cert", "signed.cert")],
        size = "small",
        srcs = [Label("//bazel:verify_ssl_cert")],
        args = ["$(location :{}_{})".format(name, f) for f in ("root_ca.cert", "signed.cert")],
        tags = [
            "local",
        ] + test_tags,
    )
