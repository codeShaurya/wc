#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/on-off-helper.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
using namespace ns3;
using namespace std;
int main (int argc, char **argv)
{
Config::SetDefault ("ns3::OnOffApplication::PacketSize",StringValue ("64"));
Config::SetDefault ("ns3::OnOffApplication::DataRate",StringValue ("1024bps"));
//Set Non-unicastMode rate to unicast mode
Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",StringValue("DsssRate2Mbps"));
Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (10));

NodeContainer nodes;
nodes.Create (3);
MobilityHelper mobility;
NodeContainer staticNodes;
NodeContainer mobileNodes;
staticNodes.Add(nodes.Get(0));
staticNodes.Add(nodes.Get(1));
mobileNodes.Add(nodes.Get(1));
staticNodes.Add(nodes.Get(2));

Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 10.0, 0.0));
  positionAlloc->Add (Vector (7.0, 20.0, 0.0));
  positionAlloc->Add (Vector (15.0, 10.0, 0.0));

  //  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
   mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");

  mobility.SetPositionAllocator (positionAlloc);

  mobility.Install (staticNodes);
  Ptr<ConstantVelocityMobilityModel> model = (mobileNodes.Get(0))->GetObject<ConstantVelocityMobilityModel>();
  model->SetVelocity(Vector(0,-1.0,0));
// setting up wifi phy and channel using helpers
WifiHelper wifi;
wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
YansWifiChannelHelper wifiChannel;
wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel");
// wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
wifiPhy.SetChannel (wifiChannel.Create ());
// Add a mac
WifiMacHelper wifiMac;
wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
"DataMode",StringValue ("DsssRate2Mbps"),
"ControlMode",StringValue ("DsssRate2Mbps"));
// wifiPhy.Set ("TxPowerStart",DoubleValue (0.01));
// wifiPhy.Set ("TxPowerEnd", DoubleValue (0.01));
wifiMac.SetType ("ns3::AdhocWifiMac");
NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);
AodvHelper aodv;
InternetStackHelper stack;
stack.SetRoutingHelper (aodv);
stack.Install (nodes);
Ipv4AddressHelper address;
address.SetBase ("10.0.0.0", "255.0.0.0");
Ipv4InterfaceContainer interfaces;
interfaces = address.Assign (devices);
OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ());
onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
//uint32_t nSinks = 4;
uint32_t port=9;
double TotalTime = 20.0;
//uint32_t i;
//for (uint32_t i = 0; i < nSinks; i++)
// {
TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
Ptr<Socket> sink = Socket::CreateSocket (nodes.Get(2), tid);
InetSocketAddress local = InetSocketAddress (interfaces.GetAddress(2), port);
sink->Bind (local);
AddressValue remoteAddress (InetSocketAddress (interfaces.GetAddress (2), port));
onoff1.SetAttribute ("Remote", remoteAddress);
//Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
ApplicationContainer temp = onoff1.Install (nodes.Get (0));
temp.Start (Seconds (1.0));
temp.Stop (Seconds (TotalTime));
//}
FlowMonitorHelper flowmon;
Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
Simulator::Stop (Seconds (TotalTime));
AnimationInterface anim("aodv.xml");


Simulator::Run ();
monitor->CheckForLostPackets ();
Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
for (map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
{
if (i->first)
{
Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
cout << " Tx Packets: " << i->second.txPackets << "\n";
cout << " Tx Bytes: " << i->second.txBytes << "\n";
cout << " TxOffered: " << i->second.txBytes * 8.0 / 9.0 / 1000 << " Kbps\n";
cout << " Rx Packets: " << i->second.rxPackets << "\n";
cout << " Rx Bytes: " << i->second.rxBytes << "\n";
cout << " Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 << " Kbps\n";
uint32_t lost= i->second.txPackets - i->second.rxPackets;
cout << " Packet Loss Ratio: " << lost*100 / i->second.txPackets << "%\n";
}
}
Simulator::Destroy ();
}