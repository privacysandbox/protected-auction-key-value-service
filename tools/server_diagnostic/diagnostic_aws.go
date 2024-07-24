// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"encoding/base64"
	"errors"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
	"tools/server_diagnostic/common"
)

// Flags
var (
	serverGrpcPortFlag             = flag.Int("server_grpc_port", 50051, "Port for KV server grpc endpoint")
	serverHttpPortFlag             = flag.Int("server_http_port", 51052, "Port for KV server http endpoint")
	regionFlag                     = flag.String("region", "us-east-1", "The region for checking the deployed server")
	environmentFlag                = flag.String("environment", "", "The deployment environment of KV server")
	toolBoxDockerImageFlag         = flag.String("toolbox_docker_image", "./diagnostic_tool_box_docker_image_amd64.tar", "The directory of the diagnostic tool box")
	toolOutputDirFlag              = flag.String("tool_output_dir", "./tools/output", "The directory of the diagnostic tool output")
	serverVerbosityFlag            = flag.Int("verbosity", 1, "The logging verbosity level of KV server running in docker")
	serverDockerImageFlag          = flag.String("server_docker_image", "/home/ec2-user/server_docker_image.tar", "The path of KV server docker image")
	serverEnclaveImageFlag         = flag.String("server_enclave_image", "/opt/privacysandbox/server_enclave_image.eif", "The path of server enclave image")
	vSockProxyFileFlag             = flag.String("vsock_proxy_file", "/opt/privacysandbox/proxy", "The path of vsock proxy file")
	protoSetForEnvoyFlag           = flag.String("envoy_protoset", "/etc/envoy/query_api_descriptor_set.pb", "The path of protoset for envoy")
	envoyConfigYamlFlag            = flag.String("envoy_config", "/etc/envoy/envoy.yaml", "The path of envoy yaml config file")
	awsOtelCollectorCtlFlag        = flag.String("otel_collector_ctl", "/opt/aws/aws-otel-collector/bin/aws-otel-collector-ctl", "The path of otel collector ctl file")
	awsOtelCollectorConfigYamlFlag = flag.String("otel_collector_config", "/opt/aws/aws-otel-collector/etc/otel_collector_config.yaml", "The path of otel collector yaml config file")
	help                           = flag.Bool("help", false, "Lists all flags and usage.")
)

const (
	systemctl                 = "/usr/bin/systemctl"
	nitroCli                  = "/usr/bin/nitro-cli"
	grpcurl                   = "/usr/bin/grpcurl"
	curl                      = "/usr/bin/curl"
	vSockProxyService         = "vsockproxy.service"
	resolvConf                = "/etc/resolv.conf"
	protoSetForTest           = "/tools/query/query_api_descriptor_set.pb"
	awsOtelCollectorService   = "aws-otel-collector.service"
	v2APIEndpoint             = "kv_server.v2.KeyValueService.GetValuesHttp"
	v2Payload                 = `{ "metadata": { "hostname": "example.com" }, "partitions": [{ "id": 0, "compressionGroupId": 0, "arguments": [{ "tags": [ "custom", "keys" ], "data": [ "hi" ] }] }] }`
	helloWorldServer          = "/tools/helloworld_server/helloworld_server"
	serverDockerProcessName   = "kv_server_docker"
	toolBoxContainerTag       = "bazel/tools/server_diagnostic:diagnostic_tool_box_docker_image"
	serverDockerLog           = "kv_server_docker.log"
	helloWorldServerDockerLog = "hello_world_server.log"
)

// ServerHealthCheckResult to save the result of multiple checks of server health
type ServerHealthCheckResult struct {
	logLocation           string
	isServerImageExisting common.Status
	isServerRunning       common.Status
	isGrpcRequestWorking  common.Status
	isHttpRequestWorking  common.Status
	isServerHealthy       bool
}

type SystemDependencyCheckResult struct {
	isVSockProxyFileExisting                    common.Status
	isVSockProxyServiceEnabled                  common.Status
	isVSockProxyServiceRunning                  common.Status
	isResolvConfExisting                        common.Status
	isVPCIpv4CidrMatchingNameServerInResolvConf common.Status
}

type EnvoyCheckResult struct {
	isProtoSetForEnvoyExisting            common.Status
	isEnvoyConfigExisting                 common.Status
	isEnvoyRunning                        common.Status
	isEnvoyListeningHttpPort              common.Status
	isTestPassedEnvoyWithHelloWorldServer common.Status
}

type AWSOtelCollectorCheckResult struct {
	isOtelCollectorCtlExisting        common.Status
	isOtelCollectorConfigYamlExisting common.Status
	isOtelCollectorServiceEnabled     common.Status
}

func main() {
	fmt.Println("Hello, this is the diagnostic tool")
	initFlag()
	if *help {
		flag.Usage()
		os.Exit(0)
	}
	diagnoseAWS()
}

// Initialize command line flags
func initFlag() {
	flag.Parse()
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage of %s:\n", os.Args[0])
		flag.PrintDefaults()
	}
}

// This function drives the diagnosis flow
func diagnoseAWS() {
	inputCheckResult := validateInputs()
	if !inputCheckResult.Ok {
		fmt.Printf("Invalid input! %v \n", inputCheckResult.Err)
		flag.Usage()
		os.Exit(1)
	}
	var summary strings.Builder
	writeInputs(&summary)
	// Check if enclave is running and healthy
	serverEnclaveHealthCheckResult := checkNitroEnclaveHealth()
	enclaveCheckTitle := "----------------------------ENCLAVE SERVER HEALTH CHECKS--------------------------------"
	serverEnclaveHealthCheckResult.writeToOutput(enclaveCheckTitle, &summary)
	if serverEnclaveHealthCheckResult.isServerHealthy {
		fmt.Println("Enclave is running healthy and server is able to process grpc and http requests!")
		return
	}
	// Server is not healthy, check system dependencies that are mandatory for
	// the server to run and process requests
	var systemDepCheckResult SystemDependencyCheckResult
	allSysDepChecksPass := checkSystemDependencies(&systemDepCheckResult)
	systemDepCheckResult.writeToOutput(&summary)
	if !allSysDepChecksPass {
		printSummary(summary.String())
		return
	}
	// All system dependency checks are passed, check the Envoy health
	envoyCheckResult := checkEnvoyHealth()
	envoyCheckResult.writeToOutput(&summary)

	// Check otel collector
	otelCheckResult := checkOtelCollector()
	otelCheckResult.writeToOutput(&summary)

	// Try to run KV server outside of enclave and check server health
	serverDockerHealthCheckResult := runServerOutsideOfEnclaveAndCheckRequests()
	serverDockerCheckTitle := "------------------------OUTSIDE OF ENCLAVE SERVER HEALTH CHECKS--------------------------"
	serverDockerHealthCheckResult.writeToOutput(serverDockerCheckTitle, &summary)
	// Print summary
	printSummary(summary.String())
}

// Validates user inputs
func validateInputs() common.Status {
	if len(strings.TrimSpace(*regionFlag)) == 0 {
		return common.Status{false, errors.New("empty region specified for checking the deployed server")}
	}
	if len(strings.TrimSpace(*environmentFlag)) == 0 {
		return common.Status{false, errors.New("empty deployment environment specified for KV server")}
	}
	if len(strings.TrimSpace(*toolOutputDirFlag)) == 0 {
		return common.Status{false, errors.New("empty output directory specified for diagnostic tool")}
	}
	if len(strings.TrimSpace(*toolBoxDockerImageFlag)) == 0 {
		return common.Status{false, errors.New("empty path of toolbox docker image")}
	}
	status := common.FileExists(*toolBoxDockerImageFlag)
	if status.Err != nil {
		return status
	}
	_, err := common.ExecuteShellCommand("docker", []string{"load", "-i", *toolBoxDockerImageFlag})
	if err != nil {
		return common.Status{false, err}
	}
	return common.Status{true, nil}
}

// Write flags name and values to the output string
func writeInputs(output *strings.Builder) {
	output.WriteString("-----------------------------------INPUT FLAGS------------------------------------------\n\n")
	flag.CommandLine.VisitAll(func(f *flag.Flag) {
		output.WriteString(fmt.Sprintf("%s: %v\n", f.Name, f.Value))
	})
}

// Prints summary
func printSummary(s string) {
	fmt.Println("-------------------------------------SUMMARY--------------------------------------------\n")
	fmt.Println(s)
}

// Writes summary of system dependency check result
func (result *SystemDependencyCheckResult) writeToOutput(output *strings.Builder) {
	output.WriteString("----------------------------SYSTEM DEPENDENCY CHECKS------------------------------------\n")
	output.WriteString("All checks must be passed for server to start and process grpc requests \n\n")
	output.WriteString(fmt.Sprintf("%s. %s exists. %s \n", result.isVSockProxyFileExisting.Result(), *vSockProxyFileFlag, result.isVSockProxyFileExisting.Error()))
	output.WriteString(fmt.Sprintf("%s. %s is running. %s \n", result.isVSockProxyServiceRunning.Result(), vSockProxyService, result.isVSockProxyServiceRunning.Error()))
	output.WriteString(fmt.Sprintf("%s. %s is enabled. %s \n", result.isVSockProxyServiceEnabled.Result(), vSockProxyService, result.isVSockProxyServiceEnabled.Error()))
	output.WriteString(fmt.Sprintf("%s. %s exists. %s \n", result.isResolvConfExisting.Result(), resolvConf, result.isResolvConfExisting.Error()))
	output.WriteString(fmt.Sprintf("%s. VPC Ipv4 Cidr matches nameserver in %s. %s \n", result.isVPCIpv4CidrMatchingNameServerInResolvConf.Result(), resolvConf, result.isVPCIpv4CidrMatchingNameServerInResolvConf.Error()))
	output.WriteString("\n")
}

// Writes summary of envoy check result
func (result *EnvoyCheckResult) writeToOutput(output *strings.Builder) {
	output.WriteString("-------------------------------ENVOY PROXY CHECKS---------------------------------------\n")
	output.WriteString("All checks must be passed for server to process http requests \n\n")
	output.WriteString(fmt.Sprintf("%s. %s exists. %s \n", result.isProtoSetForEnvoyExisting.Result(), *protoSetForEnvoyFlag, result.isProtoSetForEnvoyExisting.Error()))
	output.WriteString(fmt.Sprintf("%s. %s exists. %s \n", result.isEnvoyConfigExisting.Result(), *envoyConfigYamlFlag, result.isEnvoyConfigExisting.Error()))
	output.WriteString(fmt.Sprintf("%s. Envoy is running. %s \n", result.isEnvoyRunning.Result(), result.isEnvoyRunning.Error()))
	output.WriteString(fmt.Sprintf("%s. Envoy is listening the http port %d. %s \n", result.isEnvoyListeningHttpPort.Result(), *serverHttpPortFlag, result.isEnvoyListeningHttpPort.Error()))
	output.WriteString(fmt.Sprintf("%s. Test Envoy with hello world grpc server. %s \n", result.isTestPassedEnvoyWithHelloWorldServer.Result(), result.isTestPassedEnvoyWithHelloWorldServer.Error()))
	if result.isTestPassedEnvoyWithHelloWorldServer.Ok || result.isTestPassedEnvoyWithHelloWorldServer.Err != nil {
		output.WriteString(fmt.Sprintf("Hello world server log is located in %s/%s \n", *toolOutputDirFlag, helloWorldServerDockerLog))
	}
	output.WriteString("\n")
}

// Writes summary of otel collector check result
func (result *AWSOtelCollectorCheckResult) writeToOutput(output *strings.Builder) {
	output.WriteString("------------------------------OTEL COLLECTOR CHECKS--------------------------------------\n")
	output.WriteString("All checks must be passed for server to export metrics \n\n")
	output.WriteString(fmt.Sprintf("%s. %s exists. %s \n", result.isOtelCollectorCtlExisting.Result(), *awsOtelCollectorCtlFlag, result.isOtelCollectorCtlExisting.Error()))
	output.WriteString(fmt.Sprintf("%s. %s exists. %s \n", result.isOtelCollectorConfigYamlExisting.Result(), *awsOtelCollectorConfigYamlFlag, result.isOtelCollectorConfigYamlExisting.Error()))
	output.WriteString(fmt.Sprintf("%s. %s is enabled. %s \n", result.isOtelCollectorServiceEnabled.Result(), awsOtelCollectorService, result.isOtelCollectorServiceEnabled.Error()))
	output.WriteString("\n")
}

// Writes summary of server health check result
func (result *ServerHealthCheckResult) writeToOutput(title string, output *strings.Builder) {
	output.WriteString(title + "\n")
	output.WriteString(fmt.Sprintf("Are all checks passed? %s \n\n", strconv.FormatBool((result.isServerHealthy))))
	output.WriteString(fmt.Sprintf("%s. Server binary exists. %s \n", result.isServerImageExisting.Result(), result.isServerImageExisting.Error()))
	output.WriteString(fmt.Sprintf("%s. Test server was running. %s \n", result.isServerRunning.Result(), result.isServerRunning.Error()))
	output.WriteString(fmt.Sprintf("%s. Test grpc requests working. %s \n", result.isGrpcRequestWorking.Result(), result.isGrpcRequestWorking.Error()))
	output.WriteString(fmt.Sprintf("%s. Test http requests working. %s \n", result.isHttpRequestWorking.Result(), result.isHttpRequestWorking.Error()))
	if len(result.logLocation) != 0 {
		output.WriteString(fmt.Sprintf("Test server log is located in %s\n", result.logLocation))
	}
	output.WriteString("\n")
}

// Performs system dependency checks and returns whether all checks are passed or not
func checkSystemDependencies(result *SystemDependencyCheckResult) bool {
	allChecksPass := true
	fmt.Printf("Checking if %s file exists...\n", *vSockProxyFileFlag)
	result.isVSockProxyFileExisting = common.FileExists(*vSockProxyFileFlag)
	allChecksPass = allChecksPass && result.isVSockProxyFileExisting.Ok
	fmt.Println("Checking if vsock proxy service is running and enabled...")
	result.isVSockProxyServiceRunning = common.SystemdServiceExists(vSockProxyService)
	allChecksPass = allChecksPass && result.isVSockProxyServiceRunning.Ok
	result.isVSockProxyServiceEnabled = common.SystemdServiceIsEnabled(vSockProxyService)
	allChecksPass = allChecksPass && result.isVSockProxyServiceEnabled.Ok

	fmt.Sprintf("Checking if %s file exists...\n", resolvConf)
	result.isResolvConfExisting = common.FileExists(resolvConf)
	allChecksPass = allChecksPass && result.isResolvConfExisting.Ok
	fmt.Sprintf("Checking if VPC CIDR matched the name server defined in %s file...\n", resolvConf)
	result.isVPCIpv4CidrMatchingNameServerInResolvConf = checkVPCIpv4CidrMatchingResolvConf()
	allChecksPass = allChecksPass && result.isVPCIpv4CidrMatchingNameServerInResolvConf.Ok
	return allChecksPass
}

// Checks envoy proxy health
func checkEnvoyHealth() EnvoyCheckResult {
	var result EnvoyCheckResult
	fmt.Printf("Checking if proto set %s  for envoy proxy exists...\n", *protoSetForEnvoyFlag)
	result.isProtoSetForEnvoyExisting = common.FileExists(*protoSetForEnvoyFlag)

	fmt.Printf("Checking if envoy config %s exists...\n", *envoyConfigYamlFlag)
	result.isEnvoyConfigExisting = common.FileExists(*envoyConfigYamlFlag)

	fmt.Printf("Checking if envoy proxy docker process is running...\n")
	result.isEnvoyRunning = common.CheckDockerContainerIsRunning("envoyproxy")

	fmt.Printf("Checking if envoy proxy is listening to the correct http port %d \n", *serverHttpPortFlag)
	result.isEnvoyListeningHttpPort = common.CheckProcessListeningToPort("envoy", *serverHttpPortFlag)

	if result.isEnvoyRunning.Ok && result.isEnvoyListeningHttpPort.Ok {
		// Stop vsock proxy so that we can bring up hellworld server for the test port
		fmt.Printf("Stopping %s to bring up helloworld server to listen to the test grpc port %d \n", vSockProxyService, *serverGrpcPortFlag)
		common.StopSystemdService(vSockProxyService)
		result.isTestPassedEnvoyWithHelloWorldServer = testEnvoyWithHelloWorldServer()
		fmt.Printf("Restart and re-enable %s \n", vSockProxyService)
		common.StartAndEnableSystemdService(vSockProxyService)
	}
	return result
}

// Checks otel collector
func checkOtelCollector() AWSOtelCollectorCheckResult {
	var result AWSOtelCollectorCheckResult
	fmt.Printf("Checking if %s exists...\n", *awsOtelCollectorCtlFlag)
	result.isOtelCollectorCtlExisting = common.FileExists(*awsOtelCollectorCtlFlag)
	fmt.Printf("Checking if otel collector config %s exivvsts...\n", *awsOtelCollectorConfigYamlFlag)
	result.isOtelCollectorConfigYamlExisting = common.FileExists(*awsOtelCollectorConfigYamlFlag)
	fmt.Printf("Checking if %s is running and enabled...\n", awsOtelCollectorService)
	result.isOtelCollectorServiceEnabled = common.SystemdServiceIsEnabled(awsOtelCollectorService)
	return result
}

// Checks the health of server running inside enclave
func checkNitroEnclaveHealth() ServerHealthCheckResult {
	var result ServerHealthCheckResult
	// Check if nitro-cli is installed
	status := common.FileExists(nitroCli)
	common.AbortIfError(status)
	fmt.Printf("Checking if server enclave image exists...\n")
	result.isServerImageExisting = common.FileExists(*serverEnclaveImageFlag)
	if !result.isServerImageExisting.Ok {
		return result
	}
	fmt.Println("Checking if enclave is running...\n")
	result.isServerRunning = checkIfEnclaveIsRunning()
	if !result.isServerRunning.Ok {
		return result
	}
	fmt.Println("Enclave is running, checking grpc requests...\n")
	result.isGrpcRequestWorking = checkGrpcRequest()
	fmt.Println("Checking Http requests...\n")
	result.isHttpRequestWorking = checkHttpRequest()

	result.isServerHealthy = result.isGrpcRequestWorking.Ok &&
		result.isHttpRequestWorking.Ok
	return result
}

// Brings up server outside of enclave and run it in docker container and test the server
func runServerOutsideOfEnclaveAndCheckRequests() ServerHealthCheckResult {
	var result ServerHealthCheckResult
	fmt.Printf("Checking if the server docker image %s exists...\n", *serverDockerImageFlag)
	result.isServerImageExisting = common.FileExists(*serverDockerImageFlag)
	if !result.isServerImageExisting.Ok {
		return result
	}
	fmt.Printf("Testing server in docker outside of enclave...\n")
	fmt.Printf("Stopping %s to bring up server in docker to listen to the test grpc port %d \n", vSockProxyService, *serverGrpcPortFlag)
	common.StopSystemdService(vSockProxyService)
	testServerInDocker(&result)
	fmt.Printf("Restart and re-enable %s \n", vSockProxyService)
	common.StartAndEnableSystemdService(vSockProxyService)
	return result
}

// Checks if enclave is running
func checkIfEnclaveIsRunning() common.Status {
	output, err := common.ExecuteShellCommand(nitroCli, []string{"describe-enclaves"})
	if strings.Contains(output, "RUNNING") {
		return common.Status{true, nil}
	}
	return common.Status{false, err}
}

// Runs server in docker container and checks grpc and http requests and writes
// server log to the output directory
func testServerInDocker(result *ServerHealthCheckResult) {
	_, err := common.ExecuteShellCommand("docker", []string{"load", "-i", *serverDockerImageFlag})
	if err != nil {
		result.isServerRunning.Err = err
		return
	}
	// kill existing process if any
	common.StopAndRemoveDockerContainer(serverDockerProcessName)
	_, err = common.ExecuteShellCommand("docker", []string{"run", "--detach",
		"--network", "host", "--name", serverDockerProcessName,
		"--security-opt=seccomp=unconfined",
		"--entrypoint=/init_server_basic",
		"bazel/production/packaging/aws/data_server:server_docker_image",
		"--", fmt.Sprintf("--v=%d", *serverVerbosityFlag), "--stderrthreshold=0", fmt.Sprintf("--port=%d", *serverGrpcPortFlag)})
	time.Sleep(time.Second * 2)
	// Check if server is running
	result.isServerRunning = common.CheckDockerContainerIsRunning(serverDockerProcessName)
	common.ExecuteShellCommand("mkdir", []string{"-p", *toolOutputDirFlag})
	logPath := fmt.Sprintf("%s/%s", *toolOutputDirFlag, serverDockerLog)
	if !result.isServerRunning.Ok {
		// Output server logs
		result.logLocation = logPath
		common.WriteDockerProcessLogs(serverDockerProcessName, logPath)
		common.StopAndRemoveDockerContainer(serverDockerProcessName)
		return
	}
	// Check grpc request
	fmt.Println("Server in docker is running, checking grpc requests...")
	result.isGrpcRequestWorking = checkGrpcRequest()
	fmt.Println("Checking Http requests...")
	result.isHttpRequestWorking = checkHttpRequest()
	result.isServerHealthy = result.isGrpcRequestWorking.Ok &&
		result.isHttpRequestWorking.Ok
	// Output server logs
	result.logLocation = logPath
	common.WriteDockerProcessLogs(serverDockerProcessName, logPath)
	time.Sleep(time.Second * 2)
	common.StopAndRemoveDockerContainer(serverDockerProcessName)
}

// Checks envoy with a hello world grpc server
func testEnvoyWithHelloWorldServer() common.Status {
	fmt.Printf("Run hello world grpc server to test envoy proxy \n")
	helloWorldName := "hello_world_server"
	common.StopAndRemoveDockerContainer(helloWorldName)
	status := runToolboxDockerCommand(helloWorldServer, helloWorldName, fmt.Sprintf("--port %d", *serverGrpcPortFlag), true)
	if !status.Ok {
		return status
	}
	time.Sleep(time.Second)
	fmt.Println("Checking grpc request against hello world server...")
	status = checkGrpcRequest()
	common.ExecuteShellCommand("mkdir", []string{"-p", *toolOutputDirFlag})
	logPath := fmt.Sprintf("%s/%s", *toolOutputDirFlag, helloWorldServerDockerLog)
	if !status.Ok {
		common.WriteDockerProcessLogs(helloWorldName, logPath)
		common.StopAndRemoveDockerContainer(helloWorldName)
		return status
	}
	fmt.Println("Grpc request test is passed!")
	fmt.Println("Checking http request against hello world server")
	status = checkHttpRequest()
	common.WriteDockerProcessLogs(helloWorldName, logPath)
	time.Sleep(time.Second * 2)
	common.StopAndRemoveDockerContainer(helloWorldName)
	if !status.Ok {
		return status
	}
	return common.Status{true, nil}
}

// Check if the domain in the VPC IPV4 CIDR block matches the domain
// (DNS resolver address) defined in the etc/resolv.conf.
// The DNS resolver's address is at +2 of the vpc's base cidr block,
// e.g., KV server's default vpc base cidr block is 10.0.0.0
// so the private DNS resolver in a KV server vpc is at address 10.0.0.2.
// More details see https://github.com/privacysandbox/protected-auction-key-value-service/blob/main/docs/private_communication_aws.md

func checkVPCIpv4CidrMatchingResolvConf() common.Status {
	// Read cidr block for the deployed KV nameservers
	// aws ec2 describe-vpcs --filters Name=tag:environment,Values=<environment_name> --region=us-east-1 | jq -r .Vpcs[].CidrBlock
	fmt.Printf("Reading VPC CIDR block for deployed environment %s and region %s \n", *environmentFlag, *regionFlag)
	output, err := common.ExecuteBashCommand(
		fmt.Sprintf("aws ec2 describe-vpcs --filters Name=tag:environment,Values=%s --region=%s | jq -r .Vpcs[].CidrBlock", *environmentFlag, *regionFlag))
	if err != nil {
		return common.Status{false, err}
	}
	cidrBlock := strings.Split(output, "/")
	if len(cidrBlock) == 0 || len(cidrBlock[0]) == 0 {
		return common.Status{false, errors.New("empty Cidr block read from aws describe-vpcs")}
	}
	cidrDomainFields := strings.Split(cidrBlock[0], ".")
	if len(cidrDomainFields) != 4 {
		return common.Status{false, fmt.Errorf("invalid cidr domain in the cidr block %s, expected 4 fields seperated by periods", cidrBlock)}
	}
	cidrDomainLastField, err := strconv.Atoi(cidrDomainFields[3])
	if err != nil {
		return common.Status{false, fmt.Errorf("error parsing last field of cidr domain", err.Error())}
	}
	// Read nameservers from /etc/resolv.conf
	fmt.Printf("Reading nameservers from %s \n", resolvConf)
	nameserverResult, err := common.ExecuteBashCommand(fmt.Sprintf("cat %s  | grep -v '^#' | grep nameserver | awk '{print $2}'", resolvConf))
	if err != nil {
		return common.Status{false, err}
	}
	nameservers := strings.Split(nameserverResult, "\n")
	if len(nameservers) == 0 {
		return common.Status{false, fmt.Errorf("empty nameserver defined in the %s", resolvConf)}
	}
	fmt.Printf("Checking if cidr block domain matches the nameserver in %s \n", resolvConf)
	for _, nameserver := range nameservers {
		nameserverDomainFields := strings.Split(nameserver, ".")
		if len(nameserverDomainFields) == 4 {
			if nameserverDomainFields[0] == cidrDomainFields[0] && nameserverDomainFields[1] == cidrDomainFields[1] && nameserverDomainFields[2] == cidrDomainFields[2] && nameserverDomainFields[3] == strconv.Itoa((cidrDomainLastField+2)) {
				return common.Status{true, nil}
			}
		}
	}
	return common.Status{false, fmt.Errorf("the nameserver %s defined in the %s does not match domain name %s in the cidr block", nameserverResult, resolvConf, cidrBlock[0])}
}

// Checks grpc request against test server
func checkGrpcRequest() common.Status {
	encodedPayload := base64.StdEncoding.EncodeToString([]byte(v2Payload))
	args := fmt.Sprintf("--protoset %s -d '{\"raw_body\": {\"data\": \"%s\"}}' --plaintext localhost:%d %s", protoSetForTest, encodedPayload, *serverGrpcPortFlag, v2APIEndpoint)
	status := runToolboxDockerCommand(grpcurl, "grpcurl", args, false)
	common.RemoveDockerContainer("grpcurl")
	if status.Err != nil {
		return status
	}
	return common.Status{true, nil}
}

// Check http request against test server
func checkHttpRequest() common.Status {
	args := []string{"-vX", "PUT", "-d", v2Payload, fmt.Sprintf("http://localhost:%d", *serverHttpPortFlag) + "/v2/getvalues"}
	output, err := common.ExecuteShellCommand(curl, args)
	if err != nil {
		return common.Status{false, fmt.Errorf("http request failed! response: %s, error: %v", output, err.Error())}
	}
	return common.Status{true, nil}
}

// Executes tool box docker commands from different entry-points
func runToolboxDockerCommand(entrypoint string, name string, args string, detach bool) common.Status {
	var cmd strings.Builder
	cmd.WriteString("docker run ")
	if detach {
		cmd.WriteString("--detach ")
	}
	cmd.WriteString(fmt.Sprintf("--entrypoint=%s --network host --name %s --add-host=host.docker.internal:host-gateway %s %s", entrypoint, name, toolBoxContainerTag, args))
	_, err := common.ExecuteBashCommand(cmd.String())
	if err != nil {
		return common.Status{false, err}
	}
	return common.Status{true, nil}
}
