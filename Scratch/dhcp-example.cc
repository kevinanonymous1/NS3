#include <ns3/core-module.h>
#include <ns3/internet-apps-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/point-to-point-module.h>
#include <ns3/applications-module.h>
#include <ns3/netanim-module.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DhcpExample");

int main(int argc, char* argv[]) {
    // Command-line arguments
    bool verbose = false;
    bool tracing = false;
    std::string animFile = "dhcp-server-client-animation.xml";
    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Turn on the logs", verbose);
    cmd.AddValue("tracing", "Turn on the tracing", tracing);
    cmd.Parse(argc, argv);

    // Simulation parameters
    Time stopTime = Seconds(20.0);

    // Enable logging if verbose
    if (verbose) {
        LogComponentEnable("DhcpServer", LOG_LEVEL_ALL);
        LogComponentEnable("DhcpClient", LOG_LEVEL_ALL);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    }

    // Create nodes
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(3); // Client nodes
    NodeContainer router;
    router.Create(2); // Router nodes
    NodeContainer net(nodes, router); // Combined CSMA network
    NodeContainer p2pNodes;
    p2pNodes.Add(net.Get(4)); // Router 2
    p2pNodes.Create(1); // Echo server node

    // Configure CSMA channel
    NS_LOG_INFO("Create channels.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer devNet = csma.Install(net);

    // Configure point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices = pointToPoint.Install(p2pNodes);

    // Install internet stack
    InternetStackHelper tcpip;
    tcpip.Install(nodes);
    tcpip.Install(router);
    tcpip.Install(p2pNodes.Get(1));

    // Assign IP addresses for point-to-point link
    Ipv4AddressHelper address;
    address.SetBase("172.30.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);

    // Add static routing entry for echo server
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4> ipv4Ptr = p2pNodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4Ptr);
    staticRouting->AddNetworkRouteTo(Ipv4Address("172.30.0.0"), Ipv4Mask("/24"),
                                     Ipv4Address("172.30.1.1"), 1);

    // Configure DHCP
    NS_LOG_INFO("Setup the IP addresses and create DHCP applications.");
    DhcpHelper dhcpHelper;
    // Assign fixed IP to router (net.Get(4))
    Ipv4InterfaceContainer fixedNodes = dhcpHelper.InstallFixedAddress(devNet.Get(4),
                                                                      Ipv4Address("172.30.0.17"),
                                                                      Ipv4Mask("/24"));

    // Install DHCP server on router (net.Get(3))
    ApplicationContainer dhcpServerApp = dhcpHelper.InstallDhcpServer(devNet.Get(3),
                                                                     Ipv4Address("172.30.0.12"),
                                                                     Ipv4Address("172.30.0.0"),
                                                                     Ipv4Mask("/24"),
                                                                     Ipv4Address("172.30.0.10"), // Pool start
                                                                     Ipv4Address("172.30.0.15"), // Pool end
                                                                     Ipv4Address("172.30.0.17")); // Gateway
    DynamicCast<DhcpServer>(dhcpServerApp.Get(0))->AddStaticDhcpEntry(devNet.Get(2)->GetAddress(),
                                                                     Ipv4Address("172.30.0.14"));
    dhcpServerApp.Start(Seconds(0.0));
    dhcpServerApp.Stop(stopTime);

    // Install DHCP clients on nodes 0, 1, and 2
    NetDeviceContainer dhcpClientNetDevs;
    dhcpClientNetDevs.Add(devNet.Get(0));
    dhcpClientNetDevs.Add(devNet.Get(1));
    dhcpClientNetDevs.Add(devNet.Get(2));
    ApplicationContainer dhcpClients = dhcpHelper.InstallDhcpClient(dhcpClientNetDevs);
    dhcpClients.Start(Seconds(1.0));
    dhcpClients.Stop(stopTime);

    // Configure UDP echo server on p2p node
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(p2pNodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(stopTime);

    // Configure UDP echo client on node 1
    UdpEchoClientHelper echoClient(p2pInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(10.0));
    clientApps.Stop(stopTime);

    // Configure animation
    AnimationInterface anim(animFile);

    // Enable tracing if requested
    if (tracing) {
        csma.EnablePcapAll("dhcp-csma");
        pointToPoint.EnablePcapAll("dhcp-p2p");
    }

    // Run simulation
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime + Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
