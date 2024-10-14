// Copyright 2024 Google LLC
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

package common

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
)

const (
	systemctl = "/usr/bin/systemctl"
)

// Status to return whether an operation executes okay or not, and
// error if any
type Status struct {
	Ok  bool
	Err error
}

// Converts the status result to string
func (s *Status) Result() string {
	if s.Ok {
		return "OK"
	}
	return "FAILED"
}

// Returns formatted error message in string for a given status
func (s *Status) Error() string {
	if s.Ok || s.Err == nil {
		return ""
	}
	return fmt.Sprintf("ERROR: %v", s.Err)
}

// Checks if a given process is listening to a given port
func CheckProcessListeningToPort(process string, port int) Status {
	output, err := ExecuteBashCommand(fmt.Sprintf("sudo netstat -nlp | grep :%d | grep %s", port, process))
	if err != nil {
		return Status{false, err}
	}
	if len(output) == 0 {
		return Status{false, fmt.Errorf("no process %s is listening to the port %d", process, port)}
	}
	return Status{true, nil}
}

// Stops and removes a docker container, returns output and error if any
func StopAndRemoveDockerContainer(name string) (string, error) {
	output, err := ExecuteShellCommand("docker", []string{"stop", name})
	if err != nil {
		return output, err
	}
	return RemoveDockerContainer(name)
}

// Executes bash command and returns output and error if any
func ExecuteBashCommand(cmd string) (string, error) {
	return ExecuteShellCommand("bash", []string{"-c", cmd})
}

// Executes simple shell command, prints to the console the execution output,
// and returns output and error if any
func ExecuteShellCommand(command string, args []string) (string, error) {
	cmd := exec.Command(command, args...)
	fmt.Println(fmt.Sprintf("\nExecuting command: %s \n", cmd.String()))
	var stdBuffer bytes.Buffer
	writer := io.MultiWriter(os.Stdout, &stdBuffer)
	cmd.Stdout = writer
	cmd.Stderr = writer
	if err := cmd.Run(); err != nil {
		return stdBuffer.String(), fmt.Errorf("error %v: %s", err, stdBuffer.String())
	}
	return stdBuffer.String(), nil
}

// Removes a docker container, returns the output and error if any
func RemoveDockerContainer(name string) (string, error) {
	containerId, _ := ExecuteBashCommand(fmt.Sprintf("docker ps -a | grep %s | awk '{print $1}'", name))
	containerId = strings.TrimSpace(containerId)
	if len(containerId) != 0 {
		output, err := ExecuteShellCommand("docker", []string{"rm", containerId})
		if err != nil {
			fmt.Printf("Error removing docker container for %s, %s \n", name, containerId)
		}
		return output, err
	}
	return "", nil
}

// Checks if a docker container is running
func CheckDockerContainerIsRunning(name string) Status {
	output, err := ExecuteBashCommand(fmt.Sprintf("docker ps --no-trunc | grep %s | awk '{print $2}'", name))
	if err != nil {
		fmt.Printf("Error executing docker ps for name %s! %v. Output %s", name, err, output)
		return Status{false, err}
	}
	if len(output) == 0 {
		return Status{false, fmt.Errorf("no docker process is found for %s", name)}
	}
	return Status{true, nil}
}

// Writes a docker process's logs to a given log file, the process can be running or a stopped docker process
func WriteDockerProcessLogs(name string, log string) Status {
	containerId, err := ExecuteBashCommand(fmt.Sprintf("docker ps -a | grep %s | awk '{print $1}'", name))
	if err != nil {
		return Status{false, err}
	}
	containerId = strings.TrimSpace(containerId)
	_, err = ExecuteBashCommand(fmt.Sprintf("docker logs %s > %s 2>&1 </dev/null &", containerId, log))
	if err != nil {
		return Status{false, fmt.Errorf("Error writing %s, %v", log, err)}
	}
	return Status{true, nil}
}

// Aborts if error in status
func AbortIfError(s Status) {
	if s.Err != nil {
		panic(s.Err)
	}
}

// Checks if a file exists in the file system
func FileExists(f string) Status {
	output, err := ExecuteBashCommand(fmt.Sprintf("sudo ls %s", f))
	if err != nil || len(output) == 0 {
		return Status{false, err}
	}
	return Status{true, nil}
}

// Checks if a given systemd service exists
func SystemdServiceExists(s string) Status {
	output, err := ExecuteShellCommand(systemctl, []string{"status", s})
	if err != nil || strings.Contains(output, s+" could not be found") || !strings.Contains(output, "Active: active") {
		return Status{false, fmt.Errorf("Error %v, output %s", err, output)}
	}
	return Status{true, nil}
}

// Checks if a given systemd service is enabled
func SystemdServiceIsEnabled(s string) Status {
	_, err := ExecuteShellCommand(systemctl, []string{"is-enabled", s})
	return Status{err == nil, err}
}

// Starts and enables a given systemd service
func StartAndEnableSystemdService(s string) Status {
	_, err := ExecuteBashCommand(fmt.Sprintf("sudo %s start %s", systemctl, s))
	if err != nil {
		return Status{false, err}
	}
	_, err = ExecuteBashCommand(fmt.Sprintf("sudo %s enable %s", systemctl, s))
	return Status{err == nil, err}
}

// Stops a given systemd service
func StopSystemdService(s string) Status {
	_, err := ExecuteBashCommand(fmt.Sprintf("sudo %s stop %s", systemctl, s))
	return Status{err == nil, err}
}
