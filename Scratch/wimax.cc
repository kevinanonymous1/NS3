#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wimax-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipcs-classifier-record.h"
#include "ns3/service-flow.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    // Enable logging for debugging
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer ssNodes, bsNodes;
    ssNodes.Create (2);  // 2 subscriber stations
    bsNodes.Create (1);  // 1 base station

    // Setup WiMAX devices
    WimaxHelper wimax;
    WimaxHelper::SchedulerType scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;

    NetDeviceContainer ssDevs = wimax.Install (ssNodes, 
        WimaxHelper::DEVICE_TYPE_SUBSCRIBER_STATION,
        WimaxHelper::SIMPLE_PHY_TYPE_OFDM, scheduler);

    NetDeviceContainer bsDevs = wimax.Install (bsNodes,
        WimaxHelper::DEVICE_TYPE_BASE_STATION,
        WimaxHelper::SIMPLE_PHY_TYPE_OFDM, scheduler);

    // Configure modulation
    Ptr<SubscriberStationNetDevice> ss[2];
    for (int i = 0; i < 2; i++) {
        ss[i] = ssDevs.Get (i)->GetObject<SubscriberStationNetDevice> ();
        ss[i]->SetModulationType (WimaxPhy::MODULATION_TYPE_QAM64_34);
    }

    Ptr<BaseStationNetDevice> bs = bsDevs.Get (0)->GetObject<BaseStationNetDevice> ();

    // Install internet stack
    InternetStackHelper stack;
    stack.Install (bsNodes);
    stack.Install (ssNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer ssInterfaces = address.Assign (ssDevs);
    Ipv4InterfaceContainer bsInterface = address.Assign (bsDevs);

    // Set mobility positions
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector(15.0, 25.0, 0.0));  // SS0
    positionAlloc->Add (Vector(25.0, 15.0, 0.0));  // SS1
    positionAlloc->Add (Vector(5.0, 15.0, 0.0));   // BS

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator (positionAlloc);
    mobility.Install (ssNodes);
    mobility.Install (bsNodes);

    // Setup UDP server on SS0
    UdpServerHelper udpServer (22000);
    ApplicationContainer serverApps = udpServer.Install (ssNodes.Get (0));
    serverApps.Start (Seconds (4.0));
    serverApps.Stop (Seconds (10.0));

    // Setup UDP client on SS1
    UdpClientHelper udpClient (ssInterfaces.GetAddress (0), 22000);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (15000));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (512));

    ApplicationContainer clientApps = udpClient.Install (ssNodes.Get (1));
    clientApps.Start (Seconds (5.0));
    clientApps.Stop (Seconds (9.5));

    // Setup QoS service flows
    IpcsClassifierRecord dlClassifier (
        Ipv4Address ("0.0.0.0"), Ipv4Mask ("0.0.0.0"),
        ssInterfaces.GetAddress (0), Ipv4Mask ("255.255.255.255"),
        0, 65000, 22000, 22000, 17, 1);
    
    ServiceFlow dlServiceFlow = wimax.CreateServiceFlow (
        ServiceFlow::SF_DIRECTION_DOWN, ServiceFlow::SF_TYPE_RTPS, dlClassifier);
    ss[0]->AddServiceFlow (dlServiceFlow);

    IpcsClassifierRecord ulClassifier (
        ssInterfaces.GetAddress (1), Ipv4Mask ("255.255.255.255"),
        Ipv4Address ("0.0.0.0"), Ipv4Mask ("0.0.0.0"),
        0, 65000, 22000, 22000, 17, 1);
    
    ServiceFlow ulServiceFlow = wimax.CreateServiceFlow (
        ServiceFlow::SF_DIRECTION_UP, ServiceFlow::SF_TYPE_RTPS, ulClassifier);
    ss[1]->AddServiceFlow (ulServiceFlow);

    // NetAnim Configuration
    AnimationInterface anim ("animationWiMAX.xml");
    anim.SetConstantPosition (ssNodes.Get (0), 1.0, 3.0);
    anim.SetConstantPosition (ssNodes.Get (1), 4.0, 6.0);
    anim.SetConstantPosition (bsNodes.Get (0), 17.0, 18.0);
    anim.EnablePacketMetadata (true);

    // Enable pcap tracing
    wimax.EnablePcapAll ("WIMAX");

    Simulator::Stop (Seconds (15.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}

