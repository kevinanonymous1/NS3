#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/applications-module.h>
#include <ns3/mobility-module.h>
#include <ns3/internet-module.h>
#include <ns3/yans-wifi-helper.h>
#include <ns3/energy-module.h>
#include <ns3/wifi-radio-energy-model-helper.h>
#include <ns3/config-store-module.h>
#include <ns3/netanim-module.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetwork");

// Function to generate traffic by sending packets
static void GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval) {
    if (pktCount > 0) {
        socket->Send(Create<Packet>(pktSize));
        Simulator::Schedule(pktInterval, &GenerateTraffic, socket, pktSize, pktCount - 1, pktInterval);
    }
}

// Function to print energy status of nodes
void EnergyPrint(EnergySourceContainer source, int rounds) {
    int id = 0;
    int k = 1;
    double lowestEnergy = 50.0; // Initial high value for comparison

    for (EnergySourceContainer::Iterator c = source.Begin(); c != source.End(); ++c) {
        double energyRemain = (*c)->GetRemainingEnergy();
        if (energyRemain < lowestEnergy) {
            lowestEnergy = energyRemain;
            id = k;
        }
        NS_LOG_UNCOND("Node " << k << ": " << energyRemain << "J");
        k++;
    }
    NS_LOG_UNCOND("Rounds " << rounds << ": Node " << id << " lowest energy " << lowestEnergy << "J");
}

// Function to schedule packet sending with energy model adjustments
void SendPacket(Ptr<Socket> source[], int clusterSize, double startTime, Ptr<DeviceEnergyModel> modelPtr) {
    const double interval = 0.1;
    const uint32_t packetSize = 5000;
    const uint32_t numPackets = 1;
    Time interPacketInterval = Seconds(interval);

    for (int i = 0; i < clusterSize; ++i) {
        if (i == 1) { // Specific energy settings for node 1
            modelPtr->SetAttribute("RxCurrentA", DoubleValue(0.45));
            modelPtr->SetAttribute("TxCurrentA", DoubleValue(0.21));
            modelPtr->SetAttribute("IdleCurrentA", DoubleValue(0.1));
            modelPtr->SetAttribute("SleepCurrentA", DoubleValue(0.03));
        }

        Time time = Seconds(startTime + i * interval * 1.5);
        Simulator::Schedule(time, &GenerateTraffic, source[i], packetSize, numPackets, interPacketInterval);

        if (i == 1) { // Reset energy settings for node 1
            modelPtr->SetAttribute("RxCurrentA", DoubleValue(0.00001));
            modelPtr->SetAttribute("TxCurrentA", DoubleValue(0.00001));
            modelPtr->SetAttribute("IdleCurrentA", DoubleValue(0.00001));
            modelPtr->SetAttribute("SleepCurrentA", DoubleValue(0.00001));
        }
    }
}

// Callback function for tracing remaining energy
void Remain(double oldValue, double newValue) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s " << newValue << "J");
}

int main(int argc, char* argv[]) {
    // Simulation parameters
    const int clusterSize = 15;
    const int rounds = 5;

    // Create nodes
    Ptr<Node> sink = CreateObject<Node>();
    NodeContainer sensor;
    sensor.Create(clusterSize);
    NodeContainer allNodes;
    allNodes.Add(sink);
    allNodes.Add(sensor);

    // Set up WiFi channel
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(35.36));

    // Configure physical layer
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure MAC layer and WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate1Mbps"),
                                "ControlMode", StringValue("DsssRate1Mbps"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Install WiFi devices
    NetDeviceContainer wifiDevices = wifi.Install(phy, mac, sensor);
    NetDeviceContainer sinkDevice = wifi.Install(phy, mac, sink);
    NetDeviceContainer allDevices;
    allDevices.Add(sinkDevice);
    allDevices.Add(wifiDevices);

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobility.Install(sensor);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(25),
                                 "MinY", DoubleValue(25));
    mobility.Install(sink);

    // Set up energy model
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(5.0));
    EnergySourceContainer energySources = energySourceHelper.Install(sensor);

    WifiRadioEnergyModelHelper energyModelHelper;
    energyModelHelper.Set("TxCurrentA", DoubleValue(0.45));
    energyModelHelper.Set("RxCurrentA", DoubleValue(0.21));
    energyModelHelper.Set("IdleCurrentA", DoubleValue(0.1));
    energyModelHelper.Set("SleepCurrentA", DoubleValue(0.03));
    DeviceEnergyModelContainer energyDevices = energyModelHelper.Install(wifiDevices, energySources);

    // Trace energy for the first node
    Ptr<BasicEnergySource> sourcePtr = DynamicCast<BasicEnergySource>(energySources.Get(0));
    sourcePtr->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&Remain));
    Ptr<DeviceEnergyModel> modelPtr = energyDevices.Get(0);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(allDevices);

    // Set up sink socket
    TypeId tid = UdpSocketFactory::GetTypeId();
    Ptr<Socket> recvSink = Socket::CreateSocket(sink, tid);
    InetSocketAddress local = InetSocketAddress(interfaces.GetAddress(0), 80);
    recvSink->Bind(local);
    recvSink->SetAllowBroadcast(false);

    // Set up source sockets
    Ptr<Socket> source[clusterSize];
    for (int i = 0; i < clusterSize; ++i) {
        InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(i + 1), 80);
        source[i] = Socket::CreateSocket(sensor.Get(i), tid);
        source[i]->Bind(remote);
        source[i]->SetAllowBroadcast(false);
        source[i]->Connect(local);
    }

    // Set up animation
    AnimationInterface anim("wireless_sensor_simulation.xml");

    // Schedule packet sending for each round
    for (int i = 0; i < rounds; ++i) {
        SendPacket(source, clusterSize, 2.2 * i, modelPtr);
    }

    // Run simulation
    Simulator::Stop(Seconds(2.2 * (rounds + 1)));
    Simulator::Run();
    EnergyPrint(energySources, rounds);
    Simulator::Destroy();

    return 0;
}
