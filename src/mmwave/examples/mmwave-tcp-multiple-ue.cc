/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
*   Copyright (c) 2015, NYU WIRELESS, Tandon School of Engineering, New York University
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License version 2 as
*   published by the Free Software Foundation;
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*   Author: Marco Miozzo <marco.miozzo@cttc.es>
*           Nicola Baldo  <nbaldo@cttc.es>
*
*   Modified by: Marco Mezzavilla < mezzavilla@nyu.edu>
*                         Sourjya Dutta <sdutta@nyu.edu>
*                         Russell Ford <russell.ford@nyu.edu>
*                         Menglei Zhang <menglei@nyu.edu>
*/


#include "ns3/point-to-point-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "/h/176/yishanchra/research/5G/ns3-mmwave-5G/src/internet/model/tcp-socket-factory-impl.h"
//#include "ns3/gtk-config-store.h"

using namespace ns3;
using namespace mmwave;

/**
 * A script to simulate the DOWNLINK TCP data over mmWave links
 * with the mmWave devices and the LTE EPC.
 */
NS_LOG_COMPONENT_DEFINE ("mmWaveTCPMultipleUEExample");


class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  static int send_num = 1;
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);
  NS_LOG_DEBUG ("Sending:    " << send_num++ << "\t" << Simulator::Now ().GetSeconds ());

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

static void Rx (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << packet->GetSize () << std::endl;
}

static void SsThresholdChange (Ptr<OutputStreamWrapper> stream, uint32_t old, uint32_t now)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << old << "\t" << now << std::endl;
}

static void
RttChange (Ptr<OutputStreamWrapper> stream, Time oldRtt, Time newRtt)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldRtt.GetSeconds () << "\t" << newRtt.GetSeconds () << std::endl;
}

void

MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}


int
main (int argc, char *argv[])
{
  double simStopTime = 50;
  int nEnbs = 1;
  int nUEs = 1;
  double totalBandwidth = 500e6;
  double frequency0 = 28e9;
  int nPackets = 5e6;
  std::string congControl = "TcpCubic";
  std::string result_folder = "scripts/traces/";
  bool logging = true;
  bool movingUEs = false;
  Ipv4Address remoteHostAddr;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("numUEs", "Number of UEs", nUEs);
  cmd.AddValue("numEnbs", "Number of EnodeBs", nEnbs);
  cmd.AddValue("numPackets", "Number of packets", nPackets);
  cmd.AddValue("CongControl", "Congestion control to use", congControl);
  cmd.AddValue("ResultFolder", "Root folder to store the logs", result_folder);
  cmd.AddValue("movingUEs", "Enable moving UEs", movingUEs);
  cmd.AddValue("log", "Enable logging", logging);
  cmd.Parse (argc, argv);

  if (logging)
  {
    LogComponentEnable("Tcp5G", LOG_INFO);
    // TcpNewReno is under the base class TcpCongestionOps
    LogComponentEnable("TcpCongestionOps", LOG_INFO);
    LogComponentEnable("TcpCubic", LOG_INFO);
  }

  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (10 * 1024 * 1024));
  Config::SetDefault ("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue (10 * 1024 * 1024));
  Config::SetDefault ("ns3::LteRlcUmLowLat::MaxTxBufferSize", UintegerValue (10 * 1024 * 1024));
  
  // TCP settings
  TypeId congControlTypeId = TypeId::LookupByName("ns3::" + congControl);
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (congControlTypeId));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (200)));
  Config::SetDefault ("ns3::Ipv4L3Protocol::FragmentExpirationTimeout", TimeValue (Seconds (0.2)));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1400));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (131072*400));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (131072*400));
    
  // set to false to use the 3GPP radiation pattern (proper configuration of the bearing and downtilt angles is needed) 
  Config::SetDefault ("ns3::ThreeGppAntennaArrayModel::IsotropicElements", BooleanValue (true)); 

  // CC 0
  // 1. create MmWavePhyMacCommon object
  Ptr<MmWavePhyMacCommon> phyMacConfig0 = CreateObject<MmWavePhyMacCommon> ();
  phyMacConfig0->SetBandwidth (totalBandwidth);
  phyMacConfig0->SetCentreFrequency (frequency0);
  
  // 2. create the MmWaveComponentCarrier object
  Ptr<MmWaveComponentCarrier> cc0 = CreateObject<MmWaveComponentCarrier> ();
  cc0->SetConfigurationParameters (phyMacConfig0);
  cc0->SetAsPrimary (true);

  std::map<uint8_t, MmWaveComponentCarrier> ccMap;
  ccMap [0] = *cc0;

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  mmwaveHelper->SetCcPhyParams(ccMap);
  mmwaveHelper->SetSchedulerType ("ns3::MmWaveFlexTtiMacScheduler");
  Ptr<MmWavePointToPointEpcHelper>  epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line
  cmd.Parse (argc, argv);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create (nEnbs);
  ueNodes.Create (nUEs);

  // Install Mobility Model
  MobilityHelper enbmobility;
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 15));
  enbmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbmobility.SetPositionAllocator (enbPositionAlloc);
  enbmobility.Install (enbNodes);

  MobilityHelper uemobility;
  if (!movingUEs)
  {
    uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  }
  else
  {
    uemobility.SetMobilityModel (
      "ns3::RandomWalk2dMobilityModel",
      "Bounds", RectangleValue (Rectangle (-200, 200, -200, 200))
    );
  }

  Ptr<UniformDiscPositionAllocator> uePositionAlloc = CreateObject<UniformDiscPositionAllocator> ();
  uePositionAlloc->SetAttribute ("rho", DoubleValue (150));
  uePositionAlloc->SetAttribute ("Z", DoubleValue(1.6));
  uemobility.SetPositionAllocator (uePositionAlloc);
  uemobility.Install (ueNodes);


  // Install LTE Devices to the nodes
  NetDeviceContainer enbDevs = mmwaveHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  // Assign IP address to UEs, and install applications
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));

  mmwaveHelper->AttachToClosestEnb (ueDevs, enbDevs);
  // mmwaveHelper->EnableTraces ();

  // Set the default gateway for the UE
  for (int i = 0; i < nUEs; i++)
  {
    Ptr<Node> ueNode = ueNodes.Get (i);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
    ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
  }

  // Install and start applications on UEs and remote host
  uint16_t sinkPort = 20000;

  std::vector<Address> sinkAddresses;
  for (int i = 0; i < nUEs; i++)
  {
    Address sinkAddress (InetSocketAddress (ueIpIface.GetAddress (i), sinkPort));
    sinkAddresses.push_back(sinkAddress);
  }
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (ueNodes);
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (simStopTime));

  std::vector<Ptr<Socket>> ns3TcpSockets;
  for (int i = 0; i < nUEs; i++)
  {
    ns3TcpSockets.push_back(Socket::CreateSocket(remoteHost, TcpSocketFactory::GetTypeId()));
  }

  for (int i = 0; i < nUEs; i++)
  {
    Ptr<MyApp> app = CreateObject<MyApp> ();
    app->Setup (ns3TcpSockets[i], sinkAddresses[i], 1400, nPackets, DataRate ("2Gb/s"));
    remoteHost->AddApplication (app);

    app->SetStartTime (Seconds (0.1));
    app->SetStopTime (Seconds (simStopTime));
  }

  AsciiTraceHelper asciiTraceHelper;
  for (int i = 0; i < nUEs; i++)
  {
    std::stringstream windowFileName;
    windowFileName << result_folder << congControl << "/mmWave-tcp-window-" << i << ".txt";
    std::stringstream dataFileName;
    dataFileName << result_folder << congControl << "/mmWave-tcp-data-" << i << ".txt";
    std::stringstream ssThresholdFileName;
    ssThresholdFileName << result_folder << congControl << "/mmWave-tcp-ssthresh-" << i << ".txt";
    std::stringstream rttFileName;
    rttFileName << result_folder << congControl << "/mmWave-tcp-rtt-" << i << ".txt";

    Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream (windowFileName.str());
    ns3TcpSockets[i]->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream1));

    Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream (dataFileName.str());
    sinkApps.Get (i)->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&Rx, stream2));

    Ptr<OutputStreamWrapper> stream3 = asciiTraceHelper.CreateFileStream (ssThresholdFileName.str());
    ns3TcpSockets[i]->TraceConnectWithoutContext ("SlowStartThreshold", MakeBoundCallback (&SsThresholdChange, stream3));

    Ptr<OutputStreamWrapper> stream4 = asciiTraceHelper.CreateFileStream (rttFileName.str());
    ns3TcpSockets[i]->TraceConnectWithoutContext ("RTT", MakeBoundCallback (&RttChange, stream4));
  }

  // p2ph.EnablePcapAll ("mmwave-sgi-capture");

  Simulator::Stop (Seconds (simStopTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;

}
