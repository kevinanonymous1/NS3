/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Wireless NS-3 Simulation with Mobility, Energy Model, CSMA & P2P backbone
 * Author: Yash
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/basic-energy-source.h"
#include "ns3/wifi-radio-energy-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WirelessAnimationExample");

int main (int argc, char *argv[])
{
  uint32_t nWifi = 5;

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.Parse (argc, argv);

  // Node Containers
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);

  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  NodeContainer p2pNodes;
  p2pNodes.Add (wifiApNode.Get (0));
  p2pNodes.Create (1);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (1); // One additional CSMA node

  // WiFi setup
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  // P2P setup
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  // CSMA setup
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (10.0),
                                 "MinY", DoubleValue (10.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (p2pNodes.Get (1));
  mobility.Install (csmaNodes.Get (1));

  // Internet Stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);
  stack.Install (p2pNodes.Get (1));
  stack.Install (csmaNodes.Get (1));

  // IP Addresses
  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

  // Energy Model for AP
  Ptr<BasicEnergySource> energySource = CreateObject<BasicEnergySource> ();
  energySource->SetInitialEnergy (300.0);

  Ptr<WifiRadioEnergyModel> wifiEnergyModel = CreateObject<WifiRadioEnergyModel> ();
  wifiEnergyModel->SetEnergySource (energySource);
  energySource->AppendDeviceEnergyModel (wifiEnergyModel);

  wifiApNode.Get (0)->AggregateObject (energySource);

  // Applications
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (csmaNodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (wifiStaNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Animation
  AnimationInterface anim ("wireless-animation.xml");

  for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
  {
    anim.UpdateNodeDescription (wifiStaNodes.Get (i), "STA");
    anim.UpdateNodeColor (wifiStaNodes.Get (i), 255, 0, 0);
  }

  anim.UpdateNodeDescription (wifiApNode.Get (0), "AP");
  anim.UpdateNodeColor (wifiApNode.Get (0), 0, 255, 0);

  anim.UpdateNodeDescription (p2pNodes.Get (1), "P2P");
  anim.UpdateNodeColor (p2pNodes.Get (1), 255, 255, 0);

  anim.UpdateNodeDescription (csmaNodes.Get (1), "CSMA");
  anim.UpdateNodeColor (csmaNodes.Get (1), 0, 0, 255);

  anim.EnablePacketMetadata ();
  anim.EnableIpv4RouteTracking ("routingtable-wireless.xml", Seconds (0.0), Seconds (10.0), Seconds (1.0));
  anim.EnableWifiMacCounters (Seconds (0), Seconds (10));
  anim.EnableWifiPhyCounters (Seconds (0), Seconds (10));

  // PCAP
  pointToPoint.EnablePcapAll ("wireless-p2p");
  phy.EnablePcap ("wireless-wifi", apDevices.Get (0));
  csma.EnablePcap ("wireless-csma", csmaDevices.Get (1), true);

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

