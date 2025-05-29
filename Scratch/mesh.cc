/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2008,2009 IITP RAS
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation;
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*
* Author: Kirill Andreev <andreev@iitp.ru>
*/

#include <iostream>
#include <sstream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("TestMeshScript");

class MeshTest {
public:
    MeshTest();
    void Configure(int argc, char** argv);
    int Run();

private:
    int m_xSize, m_ySize;
    double m_step, m_randomStart, m_totalTime, m_packetInterval;
    uint16_t m_packetSize;
    uint32_t m_nIfaces;
    bool m_chan, m_pcap, m_ascii;
    std::string m_stack, m_root;
    NodeContainer nodes;
    NetDeviceContainer meshDevices;
    Ipv4InterfaceContainer interfaces;
    MeshHelper mesh;

    void CreateNodes();
    void InstallInternetStack();
    void InstallApplication();
    void Report();
};

MeshTest::MeshTest() :
    m_xSize(3), m_ySize(3), m_step(100.0),
    m_randomStart(0.1), m_totalTime(100.0),
    m_packetInterval(0.1), m_packetSize(1024),
    m_nIfaces(1), m_chan(true),
    m_pcap(false), m_ascii(false),
    m_stack("ns3::Dot11sStack"), m_root("ff:ff:ff:ff:ff:ff") {}

void MeshTest::Configure(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.AddValue("x-size", "Number of nodes in a row grid", m_xSize);
    cmd.AddValue("y-size", "Number of rows in a grid", m_ySize);
    cmd.AddValue("step", "Size of edge in grid (meters)", m_step);
    cmd.AddValue("start", "Max random start delay (sec)", m_randomStart);
    cmd.AddValue("time", "Simulation time (sec)", m_totalTime);
    cmd.AddValue("packet-interval", "Interval between packets (sec)", m_packetInterval);
    cmd.AddValue("packet-size", "Size of packets (bytes)", m_packetSize);
    cmd.AddValue("interfaces", "Number of radio interfaces", m_nIfaces);
    cmd.AddValue("channels", "Use different frequency channels", m_chan);
    cmd.AddValue("pcap", "Enable PCAP traces", m_pcap);
    cmd.AddValue("ascii", "Enable ASCII traces", m_ascii);
    cmd.AddValue("stack", "Type of protocol stack", m_stack);
    cmd.AddValue("root", "MAC address of root mesh point", m_root);
    cmd.Parse(argc, argv);
    if (m_ascii) PacketMetadata::Enable();
}

void MeshTest::CreateNodes() {
    nodes.Create(m_xSize * m_ySize);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    
    mesh = MeshHelper::Default();
    mesh.SetStackInstaller(m_stack, "Root", Mac48AddressValue(Mac48Address(m_root.c_str())));
    mesh.SetSpreadInterfaceChannels(m_chan ? MeshHelper::SPREAD_CHANNELS : MeshHelper::ZERO_CHANNEL);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(m_randomStart)));
    mesh.SetNumberOfInterfaces(m_nIfaces);
    meshDevices = mesh.Install(wifiPhy, nodes);
    
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(m_step),
                                  "DeltaY", DoubleValue(m_step),
                                  "GridWidth", UintegerValue(m_xSize),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    
    if (m_pcap) wifiPhy.EnablePcapAll("mp-");
    if (m_ascii) {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("mesh.tr"));
    }
}

void MeshTest::InstallInternetStack() {
    InternetStackHelper internetStack;
    internetStack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(meshDevices);
}

void MeshTest::InstallApplication() {
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(m_totalTime));
    
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(static_cast<uint32_t>(m_totalTime * (1 / m_packetInterval))));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(m_packetInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(m_packetSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(m_xSize * m_ySize - 1));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(m_totalTime));
}

int MeshTest::Run() {
    CreateNodes();
    InstallInternetStack();
    InstallApplication();
    Simulator::Schedule(Seconds(m_totalTime), &MeshTest::Report, this);
    Simulator::Stop(Seconds(m_totalTime));
    AnimationInterface anim("meshdemo.xml");
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

void MeshTest::Report() {
    std::cout << "Mesh simulation complete." << std::endl;
}

int main(int argc, char* argv[]) {
    MeshTest test;
    test.Configure(argc, argv);
    return test.Run();
}

