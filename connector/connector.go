/*
Copyright 2015 Google Inc. All rights reserved.

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/
package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/google/cups-connector/cups"
	"github.com/google/cups-connector/gcp"
	"github.com/google/cups-connector/lib"
	"github.com/google/cups-connector/manager"
	"github.com/google/cups-connector/monitor"
	"github.com/google/cups-connector/snmp"
	"github.com/google/cups-connector/xmpp"

	"github.com/golang/glog"
)

func main() {
	flag.Parse()
	defer glog.Flush()
	glog.Error(lib.FullName)
	fmt.Println(lib.FullName)

	config, err := lib.ConfigFromFile()
	if err != nil {
		glog.Fatal(err)
	}

	if _, err := os.Stat(config.MonitorSocketFilename); !os.IsNotExist(err) {
		if err != nil {
			glog.Fatal(err)
		}
		glog.Fatalf(
			"A connector is already running, or the monitoring socket %s wasn't cleaned up properly",
			config.MonitorSocketFilename)
	}

	cupsConnectTimeout, err := time.ParseDuration(config.CUPSConnectTimeout)
	if err != nil {
		glog.Fatalf("Failed to parse cups connect timeout: %s", err)
	}

	gcpXMPPPingTimeout, err := time.ParseDuration(config.XMPPPingTimeout)
	if err != nil {
		glog.Fatalf("Failed to parse xmpp ping timeout: %s", err)
	}
	gcpXMPPPingIntervalDefault, err := time.ParseDuration(config.XMPPPingIntervalDefault)
	if err != nil {
		glog.Fatalf("Failed to parse xmpp ping interval default: %s", err)
	}

	gcp, err := gcp.NewGoogleCloudPrint(config.GCPBaseURL, config.RobotRefreshToken, config.UserRefreshToken,
		config.ProxyName, config.GCPOAuthClientID, config.GCPOAuthClientSecret,
		config.GCPOAuthAuthURL, config.GCPOAuthTokenURL, gcpXMPPPingIntervalDefault)
	if err != nil {
		glog.Fatal(err)
	}

	xmpp, err := xmpp.NewXMPP(config.XMPPJID, config.ProxyName, config.XMPPServer, config.XMPPPort, gcpXMPPPingTimeout, gcpXMPPPingIntervalDefault, gcp.GetRobotAccessToken)
	if err != nil {
		glog.Fatal(err)
	}
	defer xmpp.Quit()

	cups, err := cups.NewCUPS(config.CopyPrinterInfoToDisplayName, config.CUPSPrinterAttributes,
		config.CUPSMaxConnections, cupsConnectTimeout, gcp.Translate)
	if err != nil {
		glog.Fatal(err)
	}
	defer cups.Quit()

	var snmpManager *snmp.SNMPManager
	if config.SNMPEnable {
		glog.Info("SNMP enabled")
		snmpManager, err = snmp.NewSNMPManager(config.SNMPCommunity, config.SNMPMaxConnections)
		if err != nil {
			glog.Fatal(err)
		}
		defer snmpManager.Quit()
	}

	pm, err := manager.NewPrinterManager(cups, gcp, xmpp, snmpManager, config.CUPSPrinterPollInterval,
		config.GCPMaxConcurrentDownloads, config.CUPSJobQueueSize, config.CUPSJobFullUsername,
		config.CUPSIgnoreRawPrinters, config.ShareScope)
	if err != nil {
		glog.Fatal(err)
	}
	defer pm.Quit()

	m, err := monitor.NewMonitor(cups, gcp, pm, config.MonitorSocketFilename)
	if err != nil {
		glog.Fatal(err)
	}
	defer m.Quit()

	glog.Errorf("Ready to rock as proxy '%s'\n", config.ProxyName)
	fmt.Printf("Ready to rock as proxy '%s'\n", config.ProxyName)

	waitIndefinitely()

	glog.Error("Shutting down")
	fmt.Println("")
	fmt.Println("Shutting down")
}

// Blocks until Ctrl-C or SIGTERM.
func waitIndefinitely() {
	ch := make(chan os.Signal)
	signal.Notify(ch, os.Interrupt, syscall.SIGTERM)
	<-ch

	go func() {
		// In case the process doesn't die very quickly, wait for a second termination request.
		<-ch
		fmt.Println("Second termination request received")
		os.Exit(1)
	}()
}
