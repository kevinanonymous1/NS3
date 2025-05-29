#include <ns3/core-module.h>
#include <ns3/applications-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/point-to-point-module.h>
#include <ns3/ipv4-global-routing-helper.h>
#include <ns3/netanim-module.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransfer");

// Simulation parameters
static const uint32_t totalTxBytes = 2000000; // Total bytes to send
static uint32_t currentTxBytes = 0; // Current bytes sent
static const uint32_t writeSize = 1040; // Size of each write (multiple of 26)
static uint8_t data[writeSize]; // Data buffer for sending

// Function declarations
void StartFlow(Ptr<Socket> localSocket, Ipv4Address servAddress, uint16_t servPort);
void WriteUntilBufferFull(Ptr<Socket> localSocket, uint32_t txSpace);

// Trace function for congestion window changes
static void CwndTracer(uint32_t oldval, uint32_t newval) {
    NS_LOG_INFO("Moving cwnd from " << oldval << " to " << newval);
}

int main(int argc, char* argv[]) {
    // Command-line arguments
    std::string animFile = "ftp-animation.xml";
    CommandLine cmd(__FILE__);
    cmd.AddValue("animFile", "File name for animation output", animFile);
    cmd.Parse(argc, argv);

    // Initialize data buffer
    for (uint32_t i = 0; i < writeSize; ++i) {
        data[i] = static_cast<uint8_t>('a' + (i % 26));
    }

    // Optional logging (commented out as in original)
    /*
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
    LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    LogComponentEnable("TcpLargeTransfer", LOG_LEVEL_ALL);
    */

    // Create nodes
    NodeContainer n0n1;
    n0n1.Create(2); // Nodes 0 and 1
    NodeContainer n1n2;
    n1n2.Add(n0n1.Get(1)); // Node 1
    n1n2.Create(1); // Node 2

    // Configure point-to-point channels
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    // Install devices
    NetDeviceContainer dev0 = p2p.Install(n0n1);
    NetDeviceContainer dev1 = p2p.Install(n1n2);

    // Install internet stack
    InternetStackHelper internet;
    internet.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ipv4.Assign(dev0);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ipInterfs = ipv4.Assign(dev1);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure packet sink on node 2 (n1n2.Get(1))
    uint16_t servPort = 50000;
    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), servPort));
    ApplicationContainer apps = sink.Install(n1n2.Get(1));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(3.0));

    // Configure TCP source on node 0 (n0n1.Get(0))
    Ptr<Socket> localSocket = Socket::CreateSocket(n0n1.Get(0), TcpSocketFactory::GetTypeId());
    localSocket->Bind();
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                  MakeCallback(&CwndTracer));
    Simulator::ScheduleNow(&StartFlow, localSocket, ipInterfs.GetAddress(1), servPort);

    // Configure tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("tcp-large-transfer.tr"));
    p2p.EnablePcapAll("tcp-large-transfer");

    // Configure animation
    AnimationInterface anim(animFile);

    // Run simulation
    Simulator::Stop(Seconds(1000.0)); // Failsafe stop time
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

// Sending application implementation
void StartFlow(Ptr<Socket> localSocket, Ipv4Address servAddress, uint16_t servPort) {
    NS_LOG_LOGIC("Starting flow at time " << Simulator::Now().GetSeconds());
    localSocket->Connect(InetSocketAddress(servAddress, servPort));
    localSocket->SetSendCallback(MakeCallback(&WriteUntilBufferFull));
    WriteUntilBufferFull(localSocket, localSocket->GetTxAvailable());
}

void WriteUntilBufferFull(Ptr<Socket> localSocket, uint32_t txSpace) {
    while (currentTxBytes < totalTxBytes && localSocket->GetTxAvailable() > 0) {
        uint32_t left = totalTxBytes - currentTxBytes;
        uint32_t dataOffset = currentTxBytes % writeSize;
        uint32_t toWrite = writeSize - dataOffset;
        toWrite = std::min(toWrite, left);
        toWrite = std::min(toWrite, localSocket->GetTxAvailable());
        int amountSent = localSocket->Send(&data[dataOffset], toWrite, 0);
        if (amountSent < 0) {
            // Blocked; will be called again when tx space is available
            return;
        }
        currentTxBytes += amountSent;
    }
    if (currentTxBytes >= totalTxBytes) {
        localSocket->Close();
    }
}
