#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/point-to-point-module.h>
#include <ns3/applications-module.h>
#include <ns3/stats-module.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PointToPointTcpSimulation");

// Custom application class for sending packets
class MyApp : public Application {
public:
    MyApp();
    virtual ~MyApp();
    static TypeId GetTypeId();
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
    virtual void StartApplication();
    virtual void StopApplication();
    void ScheduleTx();
    void SendPacket();

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
};

MyApp::MyApp()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0) {
}

MyApp::~MyApp() {
    m_socket = nullptr;
}

TypeId MyApp::GetTypeId() {
    static TypeId tid = TypeId("MyApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<MyApp>();
    return tid;
}

void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate) {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void MyApp::StartApplication() {
    m_running = true;
    m_packetsSent = 0;

    if (InetSocketAddress::IsMatchingType(m_peer)) {
        m_socket->Bind();
    } else {
        m_socket->Bind6();
    }
    m_socket->Connect(m_peer);
    SendPacket();
}

void MyApp::StopApplication() {
    m_running = false;
    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket) {
        m_socket->Close();
    }
}

void MyApp::SendPacket() {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    if (++m_packetsSent < m_nPackets) {
        ScheduleTx();
    }
}

void MyApp::ScheduleTx() {
    if (m_running) {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
}

// Trace function for congestion window changes
static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\t" << newCwnd);
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

// Trace function for packet drops
static void RxDrop(Ptr<PcapFileWrapper> file, Ptr<const Packet> p) {
    NS_LOG_UNCOND("RxDrop at " << Simulator::Now().GetSeconds());
    file->Write(Simulator::Now(), p);
}

int main(int argc, char* argv[]) {
    // Command-line arguments
    bool useV6 = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 instead of IPv4", useV6);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Add error model to receiver
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.00001));
    devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Configure addresses and ports
    uint16_t sinkPort = 8080;
    Address sinkAddress;
    Address anyAddress;
    std::string probeType;
    std::string tracePath;

    if (!useV6) {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        sinkAddress = InetSocketAddress(interfaces.GetAddress(1), sinkPort);
        anyAddress = InetSocketAddress(Ipv4Address::GetAny(), sinkPort);
        probeType = "ns3::Ipv4PacketProbe";
        tracePath = "/NodeList/*/$ns3::Ipv4L3Protocol/Tx";
    } else {
        Ipv6AddressHelper address;
        address.SetBase("2001:0000:f00d:cafe::", Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = address.Assign(devices);
        sinkAddress = Inet6SocketAddress(interfaces.GetAddress(1, 1), sinkPort);
        anyAddress = Inet6SocketAddress(Ipv6Address::GetAny(), sinkPort);
        probeType = "ns3::Ipv6PacketProbe";
        tracePath = "/NodeList/*/$ns3::Ipv6L3Protocol/Tx";
    }

    // Set up packet sink
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", anyAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    // Set up custom application
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(ns3TcpSocket, sinkAddress, 1040, 1000, DataRate("1Mbps"));
    nodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(20.0));

    // Set up tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("seventh.cwnd");
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream));

    PcapHelper pcapHelper;
    Ptr<PcapFileWrapper> file = pcapHelper.CreateFile("seventh.pcap", std::ios::out, PcapHelper::DLT_PPP);
    devices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop, file));

    // Configure Gnuplot for packet byte count
    GnuplotHelper plotHelper;
    plotHelper.ConfigurePlot("seventh-packet-byte-count",
                             "Packet Byte Count vs. Time",
                             "Time (Seconds)",
                             "Packet Byte Count",
                             "KeyBelow");
    plotHelper.PlotProbe(probeType, tracePath, "OutputBytes", "Packet Byte Count");

    // Configure file output for packet byte count
    FileHelper fileHelper;
    fileHelper.ConfigureFile("seventh-packet-byte-count", FileAggregator::FORMATTED);
    fileHelper.Set2dFormat("Time (Seconds) = %.3e\tPacket Byte Count = %.0f");
    fileHelper.WriteProbe(probeType, tracePath, "OutputBytes");

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
