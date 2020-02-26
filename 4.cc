
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/dsdv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/on-off-helper.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

using namespace ns3;
using namespace std;

/// Run single 10 seconds experiment
void experiment (bool enableCtsRts, string wifiManager)
{
  // 0. Enable or disable CTS/RTS
  UintegerValue ctsThr = (enableCtsRts ? UintegerValue (100) : UintegerValue (2200));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);

  // 1. Create 3 nodes
  NodeContainer nodes;
  nodes.Create (25);

  // 2. Place nodes somehow, this is required by every wireless simulation
  // for (uint8_t i = 0; i < 4; ++i)
  //   {
  //     nodes.Get (i)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
  //   }
  MobilityHelper mobility1;

  mobility1.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (20.0),
                                 "DeltaY", DoubleValue (20.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility1.Install(nodes);

  // 3. Create propagation loss matrix
  Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel> ();
  lossModel->SetDefaultLoss (0); // set default loss to 200 dB (no link)
  // lossModel->SetLoss (nodes.Get (1)->GetObject<MobilityModel> (), nodes.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss 0 <-> 1 to 50 dB
  // lossModel->SetLoss (nodes.Get (2)->GetObject<MobilityModel> (), nodes.Get (1)->GetObject<MobilityModel> (), 50); // set symmetric loss 0 <-> 1 to 50 dB
  // lossModel->SetLoss (nodes.Get (2)->GetObject<MobilityModel> (), nodes.Get (3)->GetObject<MobilityModel> (), 50); // set symmetric loss 2 <-> 1 to 50 dB

  // 4. Create & setup wifi channel
  Ptr<YansWifiChannel> wifiChannel = CreateObject <YansWifiChannel> ();
  wifiChannel->SetPropagationLossModel (lossModel);
  wifiChannel->SetPropagationDelayModel (CreateObject <ConstantSpeedPropagationDelayModel> ());

  // 5. Install wireless devices
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::" + wifiManager + "WifiManager");
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel);
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac"); // use ad-hoc MAC
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);


  // 6. Install TCP/IP stack & assign IP addresses
  InternetStackHelper internet;
  DsdvHelper aodv;
  internet.SetRoutingHelper (aodv);

  internet.Install (nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.0.0.0");
  ipv4.Assign (devices);

  // 7. Install applications: two CBR streams each saturating the channel
  ApplicationContainer cbrApps;
  uint16_t cbrPort = 12345;
  OnOffHelper onOffHelper1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.0.0.25"), cbrPort));
  onOffHelper1.SetAttribute ("PacketSize", UintegerValue (110));
  onOffHelper1.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // flow 1:  node 1 -> node 0
  onOffHelper1.SetAttribute ("DataRate", StringValue ("3000000bps"));
  onOffHelper1.SetAttribute ("StartTime", TimeValue (Seconds (1.000000)));
  cbrApps.Add (onOffHelper1.Install (nodes.Get (0)));

  // flow 2:  node 2 -> node 1
  OnOffHelper onOffHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.0.0.5"), cbrPort));
  onOffHelper2.SetAttribute ("PacketSize", UintegerValue (110));
  onOffHelper2.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  onOffHelper2.SetAttribute ("DataRate", StringValue ("3000000bps"));
  onOffHelper2.SetAttribute ("StartTime", TimeValue (Seconds (2.000000)));
  cbrApps.Add (onOffHelper2.Install (nodes.Get (19)));

  // flow 3:  node 2 -> node 3
  // OnOffHelper onOffHelper3 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.0.0.4"), cbrPort));
  // onOffHelper3.SetAttribute ("PacketSize", UintegerValue (1000));
  // onOffHelper3.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  // onOffHelper3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // onOffHelper3.SetAttribute ("DataRate", StringValue ("3000000bps"));
  // onOffHelper3.SetAttribute ("StartTime", TimeValue (Seconds (1.000002)));
  // cbrApps.Add (onOffHelper3.Install (nodes.Get (2)));


  uint16_t  echoPort = 9;
  UdpEchoClientHelper echoClientHelper1 (Ipv4Address ("10.0.0.1"), echoPort);
  echoClientHelper1.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientHelper1.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClientHelper1.SetAttribute ("PacketSize", UintegerValue (10));

  ApplicationContainer pingApps;

  echoClientHelper1.SetAttribute ("StartTime", TimeValue (Seconds (0.001)));
  pingApps.Add (echoClientHelper1.Install (nodes.Get (1)));

  UdpEchoClientHelper echoClientHelper2 (Ipv4Address ("10.0.0.2"), echoPort);
  echoClientHelper2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientHelper2.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClientHelper2.SetAttribute ("PacketSize", UintegerValue (10));

  echoClientHelper2.SetAttribute ("StartTime", TimeValue (Seconds (0.002)));
  pingApps.Add (echoClientHelper2.Install (nodes.Get (2)));

  UdpEchoClientHelper echoClientHelper3 (Ipv4Address ("10.0.0.4"), echoPort);
  echoClientHelper3.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientHelper3.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClientHelper3.SetAttribute ("PacketSize", UintegerValue (10));

  echoClientHelper3.SetAttribute ("StartTime", TimeValue (Seconds (0.003)));
  pingApps.Add (echoClientHelper3.Install (nodes.Get (2)));

  // 8. Install FlowMonitor on all nodes

 FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // 9. Run simulation for 10 seconds
  Simulator::Stop (Seconds (10));
AnimationInterface anim("exposed.xml");

// anim.SetConstantPosition(nodes.Get(0),0.0,0.0);
// anim.SetConstantPosition(nodes.Get(1),10.0,0.0);
// anim.SetConstantPosition(nodes.Get(2),20.0,0.0);
// anim.SetConstantPosition(nodes.Get(3),30.0,0.0);
  Simulator::Run ();

  // 10. Print per flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {

      // Duration for throughput measurement is 9.0 seconds, since
      //   StartTime of the OnOffApplication is at about "second 1"
      // and
      //   Simulator::Stops at "second 10".
     //cout<<i->first<<endl;
      if (i->first)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          cout << "  Tx Packets: " << i->second.txPackets << "\n";
          cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 2.0 / 1000 / 1000  << " Mbps\n";
          cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          cout << "  Throughput: " << i->second.rxBytes * 8.0 / 2.0 / 1000 / 1000  << " Mbps\n";
          uint32_t lost= i->second.txPackets - i->second.rxPackets;
          cout << "  Packet Loss Ratio: " << lost*100 / i->second.txPackets << "%\n";
        }
    }

  // 11. Cleanup
  Simulator::Destroy ();
}

int main (int argc, char **argv)
{
  string wifiManager ("Arf");
  CommandLine cmd;
  cmd.AddValue ("wifiManager", "Set wifi rate manager (Aarf, Aarfcd, Amrr, Arf, Cara, Ideal, Minstrel, Onoe, Rraa)", wifiManager);
  cmd.Parse (argc, argv);

  cout << "Exposed station experiment with RTS/CTS disabled:\n" << flush;
  experiment (false, wifiManager);
  cout << "------------------------------------------------\n";
  cout << "Exposed station experiment with RTS/CTS enabled:\n";
  experiment (true, wifiManager);

  return 0;
}
