#include "ns3/log.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/mobility-helper.h"
#include "ns3/wifi-net-device.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-server.h"
#include "ns3/ht-configuration.h"
#include "ns3/he-configuration.h"
#include "ns3/netanim-module.h"
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiManagerExample");

// Constants
const double NOISE_DBM_Hz = -174.0; // Noise level at 20 MHz
double noiseDbm = NOISE_DBM_Hz;
double g_intervalBytes = 0;
uint64_t g_intervalRate = 0;

// Callback for packet reception
void PacketRx(Ptr<const Packet> pkt, const Address &addr)
{
    g_intervalBytes += pkt->GetSize();
}

// Callback for rate changes
void RateChange(uint64_t oldVal, uint64_t newVal)
{
    NS_LOG_DEBUG("Change from " << oldVal << " to " << newVal);
    g_intervalRate = newVal;
}

// Structure to hold step information
struct Step
{
    double stepSize; // Step size in dBm
    double stepTime; // Step size in seconds
};

// Structure to hold Wi-Fi standard information
struct StandardInfo
{
    StandardInfo() : m_name("none") {}

    StandardInfo(std::string name, WifiStandard standard, uint16_t width, double snrLow,
                 double snrHigh, double xMin, double xMax, double yMax)
        : m_name(name), m_standard(standard), m_width(width), m_snrLow(snrLow),
          m_snrHigh(snrHigh), m_xMin(xMin), m_xMax(xMax), m_yMax(yMax)
    {
    }

    std::string m_name;     // Standard name
    WifiStandard m_standard; // Wi-Fi standard
    uint16_t m_width;       // Channel width
    double m_snrLow;        // Lowest SNR
    double m_snrHigh;       // Highest SNR
    double m_xMin;          // X minimum for plot
    double m_xMax;          // X maximum for plot
    double m_yMax;          // Y maximum for plot
};

// Function to change signal strength and report rate
void ChangeSignalAndReportRate(Ptr<FixedRssLossModel> rssModel, struct Step step, double rss,
                               Gnuplot2dDataset &rateDataset, Gnuplot2dDataset &actualDataset)
{
    NS_LOG_FUNCTION(rssModel << step.stepSize << step.stepTime << rss);
    double snr = rss - noiseDbm;
    rateDataset.Add(snr, g_intervalRate / 1e6);
    double currentRate = ((g_intervalBytes * 8) / step.stepTime) / 1e6; // Mb/s
    actualDataset.Add(snr, currentRate);
    rssModel->SetRss(rss - step.stepSize);
    NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << "; selected rate " << (g_intervalRate / 1e6)
                        << "; observed rate " << currentRate << "; setting new power to " << (rss - step.stepSize));
    g_intervalBytes = 0;
    Simulator::Schedule(Seconds(step.stepTime), &ChangeSignalAndReportRate, rssModel, step,
                        (rss - step.stepSize), rateDataset, actualDataset);
}

int main(int argc, char *argv[])
{
    // Configuration variables
    std::vector<StandardInfo> serverStandards;
    std::vector<StandardInfo> clientStandards;
    uint32_t steps;
    uint32_t rtsThreshold = 999999; // Disabled even for large A-MPDU
    uint32_t maxAmpduSize = 65535;
    double stepSize = 1; // dBm
    double stepTime = 1; // seconds
    uint32_t packetSize = 1024; // bytes
    bool broadcast = false;
    int ap1_x = 0;
    int ap1_y = 0;
    int sta1_x = 5;
    int sta1_y = 0;
    uint16_t serverNss = 1;
    uint16_t clientNss = 1;
    uint16_t serverShortGuardInterval = 800;
    uint16_t clientShortGuardInterval = 800;
    uint16_t serverChannelWidth = 20;
    uint16_t clientChannelWidth = 20;
    std::string wifiManager("Ideal");
    std::string standard("802.11a");
    StandardInfo serverSelectedStandard;
    StandardInfo clientSelectedStandard;
    bool infrastructure = false;
    uint32_t maxSlrc = 7;
    uint32_t maxSsrc = 7;

    // Command line argument parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("maxSsrc", "The maximum number of retransmission attempts for a RTS packet", maxSsrc);
    cmd.AddValue("maxSlrc", "The maximum number of retransmission attempts for a Data packet", maxSlrc);
    cmd.AddValue("rtsThreshold", "RTS threshold", rtsThreshold);
    cmd.AddValue("maxAmpduSize", "Max A-MPDU size", maxAmpduSize);
    cmd.AddValue("stepSize", "Power between steps (dBm)", stepSize);
    cmd.AddValue("stepTime", "Time on each step (seconds)", stepTime);
    cmd.AddValue("broadcast", "Send broadcast instead of unicast", broadcast);
    cmd.AddValue("serverChannelWidth", "Set channel width of the server (valid only for 802.11n or ac)", serverChannelWidth);
    cmd.AddValue("clientChannelWidth", "Set channel width of the client (valid only for 802.11n or ac)", clientChannelWidth);
    cmd.AddValue("serverNss", "Set nss of the server (valid only for 802.11n or ac)", serverNss);
    cmd.AddValue("clientNss", "Set nss of the client (valid only for 802.11n or ac)", clientNss);
    cmd.AddValue("serverShortGuardInterval", "Set short guard interval of the server (802.11n/ac/ax) in nanoseconds", serverShortGuardInterval);
    cmd.AddValue("clientShortGuardInterval", "Set short guard interval of the client (802.11n/ac/ax) in nanoseconds", clientShortGuardInterval);
    cmd.AddValue("standard", "Set standard (802.11a, 802.11b, 802.11g, 802.11n-5GHz, 802.11n-2.4GHz, 802.11ac, 802.11-holland, 802.11p-10MHz, 802.11p-5MHz, 802.11ax-5GHz, 802.11ax-2.4GHz)", standard);
    cmd.AddValue("wifiManager", "Set wifi rate manager (Aarf, Aarfcd, Amrr, Arf, Cara, Ideal, Minstrel, MinstrelHt, Onoe, Rraa)", wifiManager);
    cmd.AddValue("infrastructure", "Use infrastructure instead of adhoc", infrastructure);
    cmd.Parse(argc, argv);

    // Program description
    std::cout << std::endl
              << "This program demonstrates and plots the operation of different " << std::endl
              << "Wi-Fi rate controls on different station configurations," << std::endl
              << "by stepping down the received signal strength across a wide range" << std::endl
              << "and observing the adjustment of the rate." << std::endl
              << "Run 'wifi-manager-example --PrintHelp' to show program options." << std::endl
              << std::endl;

    // Validate configuration
    if (!infrastructure)
    {
        NS_ABORT_MSG_IF(serverNss != clientNss, "In ad hoc mode, we assume sender and receiver are similarly configured");
    }

    if (standard == "802.11b")
    {
        NS_ABORT_MSG_IF(serverChannelWidth != 22 || clientChannelWidth != 22, "Invalid channel width for standard " << standard);
        NS_ABORT_MSG_IF(serverNss != 1 || clientNss != 1, "Invalid nss for standard " << standard);
    }
    else if (standard == "802.11a" || standard == "802.11g")
    {
        NS_ABORT_MSG_IF(serverChannelWidth != 20 || clientChannelWidth != 20, "Invalid channel width for standard " << standard);
        NS_ABORT_MSG_IF(serverNss != 1 || clientNss != 1, "Invalid nss for standard " << standard);
    }
    else if (standard == "802.11n-5GHz" || standard == "802.11n-2.4GHz")
    {
        NS_ABORT_MSG_IF(serverChannelWidth != 20 && serverChannelWidth != 40, "Invalid channel width for standard " << standard);
        NS_ABORT_MSG_IF(serverNss == 0 || serverNss > 4, "Invalid nss " << serverNss << " for standard " << standard);
        NS_ABORT_MSG_IF(clientChannelWidth != 20 && clientChannelWidth != 40, "Invalid channel width for standard " << standard);
        NS_ABORT_MSG_IF(clientNss == 0 || clientNss > 4, "Invalid nss " << clientNss << " for standard " << standard);
    }
    else if (standard == "802.11ac" || standard == "802.11ax-5GHz" || standard == "802.11ax-2.4GHz")
    {
        NS_ABORT_MSG_IF(serverChannelWidth != 20 && serverChannelWidth != 40 && serverChannelWidth != 80 && serverChannelWidth != 160,
                        "Invalid channel width for standard " << standard);
        NS_ABORT_MSG_IF(serverNss == 0 || serverNss > 4, "Invalid nss " << serverNss << " for standard " << standard);
        NS_ABORT_MSG_IF(clientChannelWidth != 20 && clientChannelWidth != 40 && clientChannelWidth != 80 && clientChannelWidth != 160,
                        "Invalid channel width for standard " << standard);
        NS_ABORT_MSG_IF(clientNss == 0 || clientNss > 4, "Invalid nss " << clientNss << " for standard " << standard);
    }

    // Calculate channel rate factor
    uint32_t channelRateFactor = std::max(clientChannelWidth, serverChannelWidth) / 20;
    channelRateFactor = channelRateFactor * std::max(clientNss, serverNss);

    // Define supported Wi-Fi standards
    serverStandards.emplace_back("802.11a", WIFI_STANDARD_80211a, 20, 3, 27, 0, 30, 60);
    serverStandards.emplace_back("802.11b", WIFI_STANDARD_80211b, 22, -5, 11, -6, 15, 15);
    serverStandards.emplace_back("802.11g", WIFI_STANDARD_80211g, 20, -5, 27, -6, 30, 60);
    serverStandards.emplace_back("802.11n-5GHz", WIFI_STANDARD_80211n_5GHZ, serverChannelWidth, 3, 30, 0, 35, 80 * channelRateFactor);
    serverStandards.emplace_back("802.11n-2.4GHz", WIFI_STANDARD_80211n_2_4GHZ, serverChannelWidth, 3, 30, 0, 35, 80 * channelRateFactor);
    serverStandards.emplace_back("802.11ac", WIFI_STANDARD_80211ac, serverChannelWidth, 5, 50, 0, 55, 120 * channelRateFactor);
    serverStandards.emplace_back("802.11-holland", WIFI_STANDARD_holland, 20, 3, 27, 0, 30, 60);
    serverStandards.emplace_back("802.11p-10MHz", WIFI_STANDARD_80211p, 10, 3, 27, 0, 30, 60);
    serverStandards.emplace_back("802.11p-5MHz", WIFI_STANDARD_80211p, 5, 3, 27, 0, 30, 60);
    serverStandards.emplace_back("802.11ax-5GHz", WIFI_STANDARD_80211ax_5GHZ, serverChannelWidth, 5, 55, 0, 60, 120 * channelRateFactor);
    serverStandards.emplace_back("802.11ax-2.4GHz", WIFI_STANDARD_80211ax_2_4GHZ, serverChannelWidth, 5, 55, 0, 60, 120 * channelRateFactor);

    clientStandards.emplace_back("802.11a", WIFI_STANDARD_80211a, 20, 3, 27, 0, 30, 60);
    clientStandards.emplace_back("802.11b", WIFI_STANDARD_80211b, 22, -5, 11, -6, 15, 15);
    clientStandards.emplace_back("802.11g", WIFI_STANDARD_80211g, 20, -5, 27, -6, 30, 60);
    clientStandards.emplace_back("802.11n-5GHz", WIFI_STANDARD_80211n_5GHZ, clientChannelWidth, 3, 30, 0, 35, 80 * channelRateFactor);
    clientStandards.emplace_back("802.11n-2.4GHz", WIFI_STANDARD_80211n_2_4GHZ, clientChannelWidth, 3, 30, 0, 35, 80 * channelRateFactor);
    clientStandards.emplace_back("802.11ac", WIFI_STANDARD_80211ac, clientChannelWidth, 5, 50, 0, 55, 120 * channelRateFactor);
    clientStandards.emplace_back("802.11-holland", WIFI_STANDARD_holland, 20, 3, 27, 0, 30, 60);
    clientStandards.emplace_back("802.11p-10MHz", WIFI_STANDARD_80211p, 10, 3, 27, 0, 30, 60);
    clientStandards.emplace_back("802.11p-5MHz", WIFI_STANDARD_80211p, 5, 3, 27, 0, 30, 60);
    clientStandards.emplace_back("802.11ax-5GHz", WIFI_STANDARD_80211ax_5GHZ, clientChannelWidth, 5, 55, 0, 60, 160 * channelRateFactor);
    clientStandards.emplace_back("802.11ax-2.4GHz", WIFI_STANDARD_80211ax_2_4GHZ, clientChannelWidth, 5, 55, 0, 60, 160 * channelRateFactor);

    // Select the specified standard
    for (const auto &std : serverStandards)
    {
        if (standard == std.m_name)
        {
            serverSelectedStandard = std;
        }
    }
    for (const auto &std : clientStandards)
    {
        if (standard == std.m_name)
        {
            clientSelectedStandard = std;
        }
    }

    NS_ABORT_MSG_IF(serverSelectedStandard.m_name == "none", "Standard " << standard << " not found");
    NS_ABORT_MSG_IF(clientSelectedStandard.m_name == "none", "Standard " << standard << " not found");

    std::cout << "Testing " << serverSelectedStandard.m_name << " with " << wifiManager << " ..." << std::endl;

    NS_ABORT_MSG_IF(clientSelectedStandard.m_snrLow >= clientSelectedStandard.m_snrHigh, "SNR values in wrong order");

    steps = static_cast<uint32_t>(std::abs((clientSelectedStandard.m_snrHigh - clientSelectedStandard.m_snrLow) / stepSize) + 1);
    NS_LOG_DEBUG("Using " << steps << " steps for SNR range " << clientSelectedStandard.m_snrLow << ":" << clientSelectedStandard.m_snrHigh);

    // Create nodes
    Ptr<Node> clientNode = CreateObject<Node>();
    Ptr<Node> serverNode = CreateObject<Node>();

    // Set up plot file names
    std::string plotName = "wifi-manager-example-" + wifiManager + "-" + standard;
    std::string dataName = "wifi-manager-example-" + wifiManager + "-" + standard;
    if (standard == "802.11n-5GHz" || standard == "802.11n-2.4GHz" || standard == "802.11ac" ||
        standard == "802.11ax-5GHz" || standard == "802.11ax-2.4GHz")
    {
        std::ostringstream oss;
        oss << "-server_" << serverChannelWidth << "MHz_" << serverShortGuardInterval << "ns_" << serverNss << "SS";
        plotName += oss.str();
        dataName += oss.str();
        oss.str("");
        oss << "-client_" << clientChannelWidth << "MHz_" << clientShortGuardInterval << "ns_" << clientNss << "SS";
        plotName += oss.str();
        dataName += oss.str();
    }
    plotName += ".eps";
    dataName += ".plt";

    std::ofstream outfile(dataName.c_str());
    Gnuplot gnuplot(plotName);

    // Configure Wi-Fi settings
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue(maxSlrc));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSsrc", UintegerValue(maxSsrc));
    Config::SetDefault("ns3::MinstrelWifiManager::PrintStats", BooleanValue(true));
    Config::SetDefault("ns3::MinstrelWifiManager::PrintSamples", BooleanValue(true));
    Config::SetDefault("ns3::MinstrelHtWifiManager::PrintStats", BooleanValue(true));

    WifiHelper wifi;
    wifi.SetStandard(serverSelectedStandard.m_standard);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

    Ptr<YansWifiChannel> wifiChannel = CreateObject<YansWifiChannel>();
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    wifiChannel->SetPropagationDelayModel(delayModel);
    Ptr<FixedRssLossModel> rssLossModel = CreateObject<FixedRssLossModel>();
    wifiChannel->SetPropagationLossModel(rssLossModel);
    wifiPhy.SetChannel(wifiChannel);

    wifi.SetRemoteStationManager("ns3::" + wifiManager + "WifiManager", "RtsCtsThreshold", UintegerValue(rtsThreshold));

    // Set up network devices
    NetDeviceContainer serverDevice;
    NetDeviceContainer clientDevice;
    WifiMacHelper wifiMac;
    if (infrastructure)
    {
        Ssid ssid = Ssid("ns-3-ssid");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        clientDevice = wifi.Install(wifiPhy, wifiMac, clientNode);
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        serverDevice = wifi.Install(wifiPhy, wifiMac, serverNode);
    }
    else
    {
        wifiMac.SetType("ns3::AdhocWifiMac");
        serverDevice = wifi.Install(wifiPhy, wifiMac, serverNode);
        clientDevice = wifi.Install(wifiPhy, wifiMac, clientNode);
    }

    // Configure random number generator
    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(2);
    wifi.AssignStreams(serverDevice, 100);
    wifi.AssignStreams(clientDevice, 100);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize", UintegerValue(maxAmpduSize));
    Config::ConnectWithoutContext("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/$ns3::" + wifiManager + "WifiManager/Rate",
                                  MakeCallback(&RateChange));

    // Configure mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(ap1_x, ap1_y, 0.0));
    NS_LOG_INFO("Setting initial AP position to " << Vector(ap1_x, ap1_y, 0.0));
    positionAlloc->Add(Vector(sta1_x, sta1_y, 0.0));
    NS_LOG_INFO("Setting initial STA position to " << Vector(sta1_x, sta1_y, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(clientNode);
    mobility.Install(serverNode);

    // Set up animation
    AnimationInterface anim("wi-fi.xml");
    AnimationInterface::SetConstantPosition(serverNode, 10, 30);
    AnimationInterface::SetConstantPosition(clientNode, 10, 45);

    // Initialize datasets for plotting
    Gnuplot2dDataset rateDataset(clientSelectedStandard.m_name + "-rate selected");
    Gnuplot2dDataset actualDataset(clientSelectedStandard.m_name + "-observed");

    struct Step step;
    step.stepSize = stepSize;
    step.stepTime = stepTime;

    // Configure Wi-Fi PHY
    Ptr<NetDevice> ndClient = clientDevice.Get(0);
    Ptr<NetDevice> ndServer = serverDevice.Get(0);
    Ptr<WifiNetDevice> wndClient = ndClient->GetObject<WifiNetDevice>();
    Ptr<WifiNetDevice> wndServer = ndServer->GetObject<WifiNetDevice>();
    Ptr<WifiPhy> wifiPhyPtrClient = wndClient->GetPhy();
    Ptr<WifiPhy> wifiPhyPtrServer = wndServer->GetPhy();

    uint8_t t_clientNss = static_cast<uint8_t>(clientNss);
    uint8_t t_serverNss = static_cast<uint8_t>(serverNss);
    wifiPhyPtrClient->SetNumberOfAntennas(t_clientNss);
    wifiPhyPtrClient->SetMaxSupportedTxSpatialStreams(t_clientNss);
    wifiPhyPtrClient->SetMaxSupportedRxSpatialStreams(t_clientNss);
    wifiPhyPtrServer->SetNumberOfAntennas(t_serverNss);
    wifiPhyPtrServer->SetMaxSupportedTxSpatialStreams(t_serverNss);
    wifiPhyPtrServer->SetMaxSupportedRxSpatialStreams(t_serverNss);

    // Configure channel width and guard interval
    if (serverSelectedStandard.m_name == "802.11n-5GHz" || serverSelectedStandard.m_name == "802.11n-2.4GHz" ||
        serverSelectedStandard.m_name == "802.11ac")
    {
        wifiPhyPtrServer->SetChannelWidth(serverSelectedStandard.m_width);
        wifiPhyPtrClient->SetChannelWidth(clientSelectedStandard.m_width);
        Ptr<HtConfiguration> clientHtConfiguration = wndClient->GetHtConfiguration();
        clientHtConfiguration->SetShortGuardIntervalSupported(clientShortGuardInterval == 400);
        Ptr<HtConfiguration> serverHtConfiguration = wndServer->GetHtConfiguration();
        serverHtConfiguration->SetShortGuardIntervalSupported(serverShortGuardInterval == 400);
    }
    else if (serverSelectedStandard.m_name == "802.11ax-5GHz" || serverSelectedStandard.m_name == "802.11ax-2.4GHz")
    {
        wifiPhyPtrServer->SetChannelWidth(serverSelectedStandard.m_width);
        wifiPhyPtrClient->SetChannelWidth(clientSelectedStandard.m_width);
        wndServer->GetHeConfiguration()->SetGuardInterval(NanoSeconds(serverShortGuardInterval));
        wndClient->GetHeConfiguration()->SetGuardInterval(NanoSeconds(clientShortGuardInterval));
    }

    NS_LOG_DEBUG("Channel width " << wifiPhyPtrClient->GetChannelWidth() << " noiseDbm " << noiseDbm);
    NS_LOG_DEBUG("NSS " << wifiPhyPtrClient->GetMaxSupportedTxSpatialStreams());

    // Configure initial RSS
    noiseDbm += 10 * log10(clientSelectedStandard.m_width * 1000000);
    double rssCurrent = clientSelectedStandard.m_snrHigh + noiseDbm;
    rssLossModel->SetRss(rssCurrent);
    NS_LOG_INFO("Setting initial Rss to " << rssCurrent);

    // Schedule signal changes
    Simulator::Schedule(Seconds(0.5 + stepTime), &ChangeSignalAndReportRate, rssLossModel, step, rssCurrent,
                        rateDataset, actualDataset);

    // Set up packet socket
    PacketSocketHelper packetSocketHelper;
    packetSocketHelper.Install(serverNode);
    packetSocketHelper.Install(clientNode);

    PacketSocketAddress socketAddr;
    socketAddr.SetSingleDevice(serverDevice.Get(0)->GetIfIndex());
    if (broadcast)
    {
        socketAddr.SetPhysicalAddress(serverDevice.Get(0)->GetBroadcast());
    }
    else
    {
        socketAddr.SetPhysicalAddress(serverDevice.Get(0)->GetAddress());
    }
    socketAddr.SetProtocol(1);

    // Configure client application
    Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
    client->SetRemote(socketAddr);
    client->SetStartTime(Seconds(0.5));
    client->SetAttribute("MaxPackets", UintegerValue(0));
    client->SetAttribute("PacketSize", UintegerValue(packetSize));
    double rate = clientSelectedStandard.m_yMax * 1e6 * 1.10;
    double clientInterval = static_cast<double>(packetSize) * 8 / rate;
    NS_LOG_DEBUG("Setting interval to " << clientInterval << " sec for rate of " << rate << " bits/sec");
    client->SetAttribute("Interval", TimeValue(Seconds(clientInterval)));
    clientNode->AddApplication(client);

    // Configure server application
    Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
    server->SetLocal(socketAddr);
    server->TraceConnectWithoutContext("Rx", MakeCallback(&PacketRx));
    serverNode->AddApplication(server);

    // Run simulation
    Simulator::Stop(Seconds((steps + 1) * stepTime));
    Simulator::Run();
    Simulator::Destroy();

    // Generate Gnuplot output
    gnuplot.AddDataset(rateDataset);
    gnuplot.AddDataset(actualDataset);

    std::ostringstream xMinStr, xMaxStr, yMaxStr;
    std::string xRangeStr = "set xrange [" + std::to_string(clientSelectedStandard.m_xMin) + ":" +
                            std::to_string(clientSelectedStandard.m_xMax) + "]";
    std::string yRangeStr = "set yrange [0:" + std::to_string(clientSelectedStandard.m_yMax) + "]";
    std::string title = "Results for " + standard + " with " + wifiManager + "\\n";
    if (standard == "802.11n-5GHz" || standard == "802.11n-2.4GHz" || standard == "802.11ac" ||
        standard == "802.11ax-5GHz" || standard == "802.11ax-2.4GHz")
    {
        title += "server: width=" + std::to_string(serverSelectedStandard.m_width) + "MHz GI=" +
                 std::to_string(serverShortGuardInterval) + "ns nss=" + std::to_string(serverNss) + "\\n";
        title += "client: width=" + std::to_string(clientSelectedStandard.m_width) + "MHz GI=" +
                 std::to_string(clientShortGuardInterval) + "ns nss=" + std::to_string(clientNss);
    }

    gnuplot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
    gnuplot.SetLegend("SNR (dB)", "Rate (Mb/s)");
    gnuplot.SetTitle(title);
    gnuplot.SetExtra(xRangeStr);
    gnuplot.AppendExtra(yRangeStr);
    gnuplot.AppendExtra("set key top left");
    gnuplot.GenerateOutput(outfile);
    outfile.close();

    return 0;
}
