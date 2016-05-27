/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include <vector>
using namespace ns3;
uint32_t qsize = 0;
bool printRTT = false;
bool printQueue = false;

std::vector<int64_t> rtt_records;
void queue_callback(uint32_t oldValue, uint32_t newValue) {
   if (printQueue) {
     std::cout << "Packets in queue:" << newValue << ":at time:" << ns3::Simulator::Now().GetMicroSeconds() << std::endl; 
   }
   qsize = newValue;
}

void trace_rtt(int64_t rtt) {
  if (printRTT) {
   std::cout << "RTT:" << rtt << ":at time:" << ns3::Simulator::Now().GetMicroSeconds() << std::endl; 
  }
   rtt_records.push_back(rtt);
} 

uint32_t getQSize() { return qsize; }

NS_LOG_COMPONENT_DEFINE ("CsmaBridgeExample");

int 
main (int argc, char *argv[])
{
  //
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  //
#if 1 
  LogComponentEnable ("CsmaBridgeExample", LOG_LEVEL_INFO);
#endif
//  double simulationTime = 10; //seconds
  std::string transportProt = "Tcp";
  std::string socketType;
  std::string cc = "";
  uint32_t queueSize = 500000;
  double emwa = 0.1, addstep = 4.0, beta = 0.05, thigh = 4000, tlow = 250;
  std::string bw = "100Mbps";
  std::string pd = "1us";
  bool useOracle = false, traceRTT = true;
  CommandLine cmd;
  cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, Udp", transportProt);
  cmd.AddValue ("cc", "Congestion control protocol to use", cc);
  cmd.AddValue("queueSize", "Size of the buffer queue", queueSize);
  cmd.AddValue("bw", "Bandwidth of links, with units", bw);
  cmd.AddValue("pd", "Propogation Delay of links, with units", pd);
  cmd.AddValue("emwa", "Timely EMWA weight", emwa);
  cmd.AddValue("addstep", "Timely Additive Increase", addstep);
  cmd.AddValue("beta", "Timely Multiplicative Decrease", beta);
  cmd.AddValue("thigh", "RTT High threshold", thigh);
  cmd.AddValue("tlow", "RTT Low threshold", tlow);
  cmd.AddValue("oracle", "Use queue occupancy for cc", useOracle);
  cmd.AddValue("trace-rtt", "Trace RTT", traceRTT);
  cmd.AddValue("printRTT", "Print RTT", printRTT);
  cmd.AddValue("printQueue", "Print Queue Occupancy", printQueue);
  cmd.Parse (argc, argv);
  
  Config::SetDefault ("ns3::Queue::MaxPackets", UintegerValue(queueSize));
 
  socketType = "ns3::TcpSocketFactory";

  if (cc.compare ("Timely") == 0) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpTimely::GetTypeId ()));
    Config::SetDefault("ns3::TcpTimely::UseOracle", BooleanValue(useOracle));
    Config::SetDefault("ns3::TcpTimely::QSizeCallback", CallbackValue(MakeCallback(&getQSize)));
    Config::SetDefault("ns3::TcpTimely::EMWA", DoubleValue(emwa));
    Config::SetDefault("ns3::TcpTimely::Addstep", DoubleValue(addstep));
    Config::SetDefault("ns3::TcpTimely::Beta", DoubleValue(beta));
    Config::SetDefault("ns3::TcpTimely::THigh", DoubleValue(thigh));
    Config::SetDefault("ns3::TcpTimely::TLow", DoubleValue(tlow));
  
  } else if (cc.compare ("Veno") == 0) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVeno::GetTypeId ()));
  
  } else if (cc.compare ("NewReno") == 0) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
  
  } else if (cc.compare ("HighSpeed") == 0) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHighSpeed::GetTypeId ()));

  } else if (cc.compare ("WestWood") == 0) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
  
  } else if (cc.compare ("Vegas") == 0) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVegas::GetTypeId ()));
  
  } else if (cc.compare ("Bic") == 0) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpBic::GetTypeId ()));
  }
  
  if (traceRTT) {
    Config::SetDefault("ns3::TcpCongestionOps::TraceRTTCallback", CallbackValue(MakeCallback(&trace_rtt)));
  }

  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //

  //
  // Explicitly create the nodes required by the topology (shown above).
  //
  NS_LOG_INFO ("Create 11 nodes.");
  NodeContainer terminals;
  terminals.Create (11);

  NodeContainer csmaSwitch;
  csmaSwitch.Create (1);

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue (bw));
  csma.SetChannelAttribute ("Delay", StringValue (pd));

  // Create the csma links, from each terminal to the switch
  NetDeviceContainer terminalDevices;
  NetDeviceContainer switchDevices;

  for (int i = 0; i < 11; i++)
    {
      NetDeviceContainer link = csma.Install (NodeContainer (terminals.Get (i), csmaSwitch));
      terminalDevices.Add (link.Get (0));
      switchDevices.Add (link.Get (1));
    }

  // Create the bridge netdevice, which will do the packet switching
  Ptr<Node> switchNode = csmaSwitch.Get (0);
  BridgeHelper bridge;
  bridge.Install (switchNode, switchDevices);

  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.Install (terminals);

  // We've got the "hardware" in place.  Now we need to add IP addresses.
  //
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (terminalDevices);

  //
  // Create an OnOff application to send TCP datagrams from node 1 to node 0.
  //
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 50000;
  uint16_t src_port_base = 10000;
  
  // Create an optional packet sink to receive these packets
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  ApplicationContainer sink_container = sink.Install (terminals.Get (0));
  sink_container.Start (Seconds (0.0));
  
  
  int maxBytes = 0;
  
  ApplicationContainer src_apps;
  AddressValue destAddr (InetSocketAddress (Ipv4Address("10.1.1.1"), port));

  for (int i = 1; i < 11; i++) {
    //
    // Create a similar flow from n3 to n0, starting at time 1.1 seconds
    //
    
    std::stringstream ss;
    ss << "10.1.1.";
    ss << i + 1;
    
    BulkSendHelper app ("ns3::TcpSocketFactory",
                            InetSocketAddress (Ipv4Address(ss.str().c_str()),
                                               src_port_base + i));
    // Set the amount of data to send in bytes.  Zero is unlimited.
    app.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    app.SetAttribute ("Remote", destAddr);
    src_apps.Add (app.Install (terminals.Get (i)));
    
  }
  src_apps.Start (Seconds (1.1));
  src_apps.Stop (Seconds (10.0));


  NS_LOG_INFO ("Configure Tracing.");
  
  Ptr<NetDevice> nd = switchDevices.Get(0);
  Ptr<CsmaNetDevice> outgoingPort = DynamicCast<CsmaNetDevice>(nd);
  Ptr<Queue> q = outgoingPort->GetQueue();
  q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&queue_callback));
  
 
  //
  // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
  // Trace output will be sent to the file "csma-bridge.tr"
  //
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("csma-bridge.tr"));

  //
  // Also configure some tcpdump traces; each interface will be traced.
  // The output files will be named:
  //     csma-bridge-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  //
  csma.EnablePcapAll ("csma-bridge", false);

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
  
  NS_LOG_INFO("Test is over.\n");
  double thr = 0;
  uint32_t totalPacketsThr = DynamicCast<PacketSink>(sink_container.Get(0))->GetTotalRx ();
  thr = totalPacketsThr * 8 / (9 * 1000000.0);
  std::stringstream ss;
  ss << "Average throughput: " << thr << " Mbit/s" << std::endl;
  if (traceRTT) {
    ss << "95th percentile RTT: ";
    std::sort(rtt_records.begin(), rtt_records.end());
    ss << rtt_records.at((int)(rtt_records.size() * 0.95)) << std::endl;
  }
  NS_LOG_INFO(ss.str().c_str());
}
