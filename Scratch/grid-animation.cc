#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/point-to-point-module.h>
#include <ns3/applications-module.h>
#include <ns3/point-to-point-layout-module.h>
#include <ns3/netanim-module.h>

using namespace ns3;

int main(int argc, char* argv[]) {
    // Set default configuration for OnOffApplication
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(512));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("500kb/s"));

    // Simulation parameters
    uint32_t xSize = 5;
    uint32_t ySize = 5;
    std::string animFile = "grid-animation.xml";

    // Parse command-line arguments
    CommandLine cmd;
    cmd.AddValue("xSize", "Number of rows of nodes", xSize);
    cmd.AddValue("ySize", "Number of columns of nodes", ySize);
    cmd.AddValue("animFile", "File name for animation output", animFile);
    cmd.Parse(argc, argv);

    // Validate grid size
    if (xSize < 1 || ySize < 1 || (xSize < 2 && ySize < 2)) {
        NS_FATAL_ERROR("Grid must have at least one node, and at least one dimension must be >= 2.");
    }

    // Configure point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create grid topology
    PointToPointGridHelper grid(xSize, ySize, pointToPoint);

    // Install internet stack
    InternetStackHelper stack;
    grid.InstallStack(stack);

    // Assign IP addresses
    grid.AssignIpv4Addresses(Ipv4AddressHelper("10.1.1.0", "255.255.255.0"),
                            Ipv4AddressHelper("10.2.1.0", "255.255.255.0"));

    // Configure On/Off application
    OnOffHelper clientHelper("ns3::UdpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    // Set up client application to send packets from (0,0) to (xSize-1, ySize-1)
    ApplicationContainer clientApps;
    AddressValue remoteAddress(InetSocketAddress(grid.GetIpv4Address(xSize - 1, ySize - 1), 1000));
    clientHelper.SetAttribute("Remote", remoteAddress);
    clientApps.Add(clientHelper.Install(grid.GetNode(0, 0)));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(1.5));

    // Configure animation
    grid.BoundingBox(1, 1, 100, 100);
    AnimationInterface anim(animFile);

    // Run simulation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
