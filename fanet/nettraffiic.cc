#include "nettraffiic.h"

#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/integer.h"
#include "ns3/wave-bsm-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/v4ping.h"
#include "ns3/arp-l3-protocol.h"
#include "ns3/arp-header.h"
#include "ns3/arp-queue-disc-item.h"

#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_OBJECT_ENSURE_REGISTERED (UdpCbrTraffic);
NS_OBJECT_ENSURE_REGISTERED (PingTraffic);
NS_OBJECT_ENSURE_REGISTERED (L3NodesDiscoverTraffic);
NS_OBJECT_ENSURE_REGISTERED (UdpClientServerTraffic);

//==================================================

static const double TIME_STEP = 0.03;

NetTraffic::NetTraffic()
{

}

NetTraffic::~NetTraffic()
{

}

void NetTraffic::SetSimulationTime(double t)
{
  m_total_time = t;
}

uint64_t NetTraffic::GetStreamIndex() const
{
  return m_sindex;
}

double NetTraffic::GetTotalSimTime() const
{
  return m_total_time;
}

//Ping
PingTraffic::PingTraffic()
{

}

NetTraffic* PingTraffic::Clone() const
{
  return new PingTraffic();
}

PingTraffic::~PingTraffic(){}

int PingTraffic::ConfigreTracing()
{
  return 1;
}

uint32_t PingTraffic::Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time)
{
  if(nc.GetN() != ip_c.GetN())
  {
    return -1;
  }

  m_sindex = stream_index;

  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
  var->SetStream(m_sindex);

  auto src = ip_c.Get(0);
  auto node = src.first->GetNetDevice(src.second)->GetNode();
  std::string s = Names::FindName(node);

  for(uint32_t i = 1; i < ip_c.GetN(); i++)
  {
    V4PingHelper hlp(ip_c.GetAddress(i));
    hlp.SetAttribute("Verbose", BooleanValue(true));
    hlp.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ApplicationContainer a = hlp.Install(node);
    a.Start(Seconds(start_time + var->GetValue(GetTotalSimTime()*0.1, GetTotalSimTime()*0.2)));
    a.Stop(Seconds(GetTotalSimTime()));

    auto tmp_node = nc.Get(i);
    std::string s2 = Names::FindName(tmp_node);
    Ptr<PingTracer> tmp = CreateObject<PingTracer>();
    tmp->CreateOutput("ping-" + s + "-to-" + s2 + ".csv");
    a.Get(0)->TraceConnectWithoutContext("Rtt", MakeCallback(&PingTracer::RttPingCb, tmp));
    m_ping_trace.push_back(tmp);
  }

  return (m_sindex += ip_c.GetN());
}

//================================

//UdpCbrTraffic
UdpCbrTraffic::UdpCbrTraffic() : m_interval(1.0), m_pckt_size(64), m_pckt_tracer(nullptr), m_start(0)
{

}
UdpCbrTraffic::~UdpCbrTraffic()
{

}

NetTraffic* UdpCbrTraffic::Clone() const
{
  return new UdpCbrTraffic();
}

uint32_t UdpCbrTraffic::Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time)
{
  if(nc.GetN() != ip_c.GetN())
  {
    return -1;
  }

  m_sindex = stream_index;
  m_start = start_time;

  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
  var->SetStream(m_sindex++);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  auto src_ifs = ip_c.Get(var->GetInteger(0, ip_c.GetN() - 1));
  Ptr<Node> src_node = src_ifs.first->GetNetDevice(src_ifs.second)->GetNode();
  std::string src_name_node = Names::FindName(src_node);

  uint16_t cnter = 1;
  var->SetStream(m_sindex++);
  for(auto it = ip_c.Begin(); it != ip_c.End(); it++)
  {
    if( (*it) == src_ifs )
    {
      continue;
    }

    Ptr<Node> n = it->first->GetNetDevice(it->second)->GetNode();
    std::string n_name = Names::FindName(n);
    Ipv4Address rem = it->first->GetAddress(it->second, 0).GetLocal();

    //Setup rx
    Ptr<Socket> recvSink = Socket::CreateSocket (n, tid);
    InetSocketAddress local = InetSocketAddress (rem, 9);
    recvSink->Bind (local);
    recvSink->SetRecvCallback(MakeCallback(&UdpCbrTraffic::RxCb, this));

    //Setup send from node 0
    InetSocketAddress remote = InetSocketAddress (rem, 9);
    Ptr<Socket> source_socket = Socket::CreateSocket (src_node, tid);
    source_socket->Connect(remote);
    m_pair_sockets.push_back(std::make_pair(source_socket, recvSink));

    Simulator::ScheduleWithContext (src_node->GetId (),
                                    Seconds (m_start + var->GetValue(0.05, 0.4)), &UdpCbrTraffic::GenerateTraffic,
                                    this, source_socket);
    cnter++;
  }

  return m_sindex;
}

int UdpCbrTraffic::ConfigreTracing()
{
  //Create trace object
  m_pckt_tracer = CreateObject<PDRAndThroughputMetr>();
  //

  m_pckt_tracer->SetSocketsPair(m_pair_sockets);
  m_pckt_tracer->CreateOutput("pdr-udp-cbr-traffic.csv");
  m_pckt_tracer->SetDumpInterval(m_interval, m_start);
  return 0;
}

void UdpCbrTraffic::RxCb(ns3::Ptr<ns3::Socket> socket)
{
  Ptr<Packet> packet;
  Address srcAddress;
  while ((packet = socket->RecvFrom (srcAddress)))
  {
    // application data, for goodput
    this->m_pckt_tracer->RxCb(packet->Copy(), socket);
  }
}

void UdpCbrTraffic::GenerateTraffic(ns3::Ptr<ns3::Socket> socket)
{
  Ptr<Packet> p = Create<Packet>(m_pckt_size);
  socket->Send (p);
  this->m_pckt_tracer->TxCb(p->Copy(), socket);
  Simulator::Schedule (Seconds(m_interval), &UdpCbrTraffic::GenerateTraffic,
                       this, socket);
}

//================================

const uint16_t L3NodesDiscoverTraffic::PROT_NUMBER = 0x0f0f;

//L3NodesDiscoverTraffic
L3NodesDiscoverTraffic::L3NodesDiscoverTraffic() : m_interval(0.1), m_pckt_size(16)
{

}
L3NodesDiscoverTraffic::~L3NodesDiscoverTraffic()
{

}

NetTraffic* L3NodesDiscoverTraffic::Clone() const
{
  return new L3NodesDiscoverTraffic();
}

uint32_t L3NodesDiscoverTraffic::Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time)
{
  if(nc.GetN() != ip_c.GetN() || (devs.GetN() != nc.GetN()))
  {
    return -1;
  }
  m_sindex = stream_index;
  m_devs = devs;
  m_start = start_time;
  //Random variable for start discovering
  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
  var->SetStream(m_sindex);

  uint16_t cnter = 1;
  double start = 0.5 + m_start;
  for(auto ip_it = ip_c.Begin(); ip_it != ip_c.End(); ip_it++)
  {
    ns3::Ptr<NetDevice> dev = ip_it->first->GetNetDevice(ip_it->second);
    Ptr<Node> n = dev->GetNode();

    Ptr<TrafficControlLayer> tc = n->GetObject<TrafficControlLayer> ();
    n->RegisterProtocolHandler(MakeCallback (&TrafficControlLayer::Receive, tc),
                               PROT_NUMBER, dev);
    tc->RegisterProtocolHandler(MakeCallback(&L3NodesDiscoverTraffic::ReceiveCb, this),
                                PROT_NUMBER, dev);
    ns3::Simulator::Schedule(ns3::Seconds(start + var->GetValue(0.0, 0.2)), &L3NodesDiscoverTraffic::TransmitCb, this, dev, tc, ip_it->first->GetAddress(ip_it->second, 0));
    cnter++;
  }

  return (m_sindex += ip_c.GetN());
}

void L3NodesDiscoverTraffic::TransmitCb(ns3::Ptr<ns3::NetDevice> dev, ns3::Ptr<ns3::TrafficControlLayer> tc, ns3::Ipv4InterfaceAddress ip_addr)
{
  ArpHeader hdr;
  hdr.SetRequest (dev->GetAddress (), ip_addr.GetLocal(), dev->GetBroadcast (), ip_addr.GetBroadcast());
  ns3::Ptr<Packet> p = Create<Packet>(m_pckt_size);
  Ptr<ArpQueueDiscItem> item = Create<ArpQueueDiscItem>(p, dev->GetBroadcast (), PROT_NUMBER, hdr);
  tc->Send(dev, item);

  ns3::Simulator::Schedule(ns3::Seconds(m_interval), &L3NodesDiscoverTraffic::TransmitCb, this, dev, tc, ip_addr);
}

void L3NodesDiscoverTraffic::ReceiveCb(ns3::Ptr<ns3::NetDevice> device, ns3::Ptr<const ns3::Packet> p, uint16_t protocol, const ns3::Address &from,
               const ns3::Address &to, ns3::NetDevice::PacketType packetType )
{
  if(m_adj_tracer)
  {
    m_adj_tracer->RxCb(device, from, p->Copy());
  }
}

int L3NodesDiscoverTraffic::ConfigreTracing()
{
  m_adj_tracer = CreateObject<AdjTracer>();
  m_adj_tracer->CreateOutput("potential.csv");
  m_adj_tracer->SetNodeDevices(m_devs);
  m_adj_tracer->SetDumpInterval(1.0, m_start);
  return 0;
}

//===================================

UdpClientServerTraffic::UdpClientServerTraffic() : m_interval(1.0), m_pckt_size(64)
{

}
UdpClientServerTraffic::~UdpClientServerTraffic()
{

}

NetTraffic* UdpClientServerTraffic::Clone() const
{
  return new UdpClientServerTraffic();
}

uint32_t UdpClientServerTraffic::Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time)
{
  if(nc.GetN() != ip_c.GetN())
  {
    return -1;
  }

  m_sindex = stream_index;
  m_start = start_time;

  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
  var->SetStream(m_sindex++);

  uint16_t port = 109;  // well-known echo port number

  auto src_ifs = ip_c.Get(var->GetInteger(0, ip_c.GetN() - 1));
  Ptr<Node> src_node = src_ifs.first->GetNetDevice(src_ifs.second)->GetNode();
  std::string src_name_node = Names::FindName(src_node);

  NodeContainer src_node_c(src_node);
  NodeContainer dst_node_c;

  for (auto ipf = ip_c.Begin() + 1; ipf != ip_c.End(); ipf++)
  {
    if( (*ipf) == src_ifs )
    {
      continue;
    }

    ApplicationContainer apps;
    uint32_t iid = (*ipf).second;
    Ptr<Node> dst_node = (*ipf).first->GetNetDevice(iid)->GetNode();
    dst_node_c.Add(dst_node);
    std::string dst_name_node = Names::FindName(dst_node);
    Ipv4Address dst_addr = (*ipf).first->GetAddress(iid,0).GetLocal();

    //Only one source node
    UdpClientHelper client(dst_addr, port);
    client.SetAttribute ("Interval", TimeValue(Seconds(m_interval)));
    client.SetAttribute ("PacketSize", UintegerValue (m_pckt_size));
    apps = client.Install(src_node_c);
    var->SetStream(m_sindex++);
    apps.Start(Seconds (m_start + var->GetValue(0.05, 0.4)));
  }

  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (dst_node_c);
  apps.Start (Seconds (0.0));

  return m_sindex;
}

int UdpClientServerTraffic::ConfigreTracing()
{
  return 0;
}

// ==================== PingPongTraffic 实现 ====================

NS_OBJECT_ENSURE_REGISTERED (PingPongTraffic);

TypeId PingPongTraffic::GetTypeId (void)
{
  static TypeId tid = TypeId ("PING_PONG")
    .SetParent<NetTraffic> ()
    .AddConstructor<PingPongTraffic> ();
  return tid;
}

PingPongTraffic::PingPongTraffic() : m_port(9999), m_total_replies(0), m_expected_replies(0)
{
}

PingPongTraffic::~PingPongTraffic()
{
}

NetTraffic* PingPongTraffic::Clone() const
{
  return new PingPongTraffic();
}

uint32_t PingPongTraffic::Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time)
{
  m_sindex = stream_index;
  m_expected_replies = nc.GetN() - 1;
  m_total_replies = 0;

  // 记录 IP 到 节点 ID 的映射，方便打印 "nodeX"
  for (uint32_t i = 0; i < nc.GetN(); ++i) {
      m_ip_to_node_id[ip_c.GetAddress(i, 0)] = i;
  }

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  // 初始化 Node 0 (发令者)
  Ptr<Node> node0 = nc.Get(0);
  Ipv4Address node0_ip = ip_c.GetAddress(0, 0);
  m_node0_socket = Socket::CreateSocket(node0, tid);
  m_node0_socket->Bind(InetSocketAddress(node0_ip, m_port));
  m_node0_socket->SetRecvCallback(MakeCallback(&PingPongTraffic::Node0RxCb, this));

  // 初始化其他节点 (接收并回复者)
  for (uint32_t i = 1; i < nc.GetN(); ++i) {
      Ptr<Node> node_i = nc.Get(i);
      Ipv4Address node_i_ip = ip_c.GetAddress(i, 0);
      Ptr<Socket> socket_i = Socket::CreateSocket(node_i, tid);
      socket_i->Bind(InetSocketAddress(node_i_ip, m_port));
      socket_i->SetRecvCallback(MakeCallback(&PingPongTraffic::NodeIRxCb, this));
      m_other_sockets[i] = socket_i;

      // 【核心防碰撞设计】：让 node0 依次向其他节点发包（每隔 0.5 秒发一个）
      // 如果同时向 100 个节点发包，会导致 AODV 路由发现风暴，网络直接瘫痪
      Simulator::Schedule(Seconds(start_time + i * 0.5), &PingPongTraffic::SendFromNode0, this, node_i_ip, i);
  }

  // 挂载到底层 IP 协议，拦截并记录 TTL 来计算跳数
  Config::Connect("/NodeList/*/$ns3::Ipv4L3Protocol/LocalDeliver", MakeCallback(&PingPongTraffic::IpLocalDeliverCb, this)); // 注意：把原本这里的WithoutContext删掉
  Config::Connect("/NodeList/*/$ns3::Ipv4L3Protocol/UnicastForward", MakeCallback(&PingPongTraffic::IpUnicastForwardCb, this));
  Config::Connect("/NodeList/*/$ns3::Ipv4L3Protocol/Drop", MakeCallback(&PingPongTraffic::IpDropCb, this));

  return m_sindex;
}

int PingPongTraffic::ConfigreTracing()
{
  return 0; // 这个特殊流量我们直接 cout 打印，不需要额外的 tracer 输出 csv
}

void PingPongTraffic::SendFromNode0(Ipv4Address dst, uint32_t node_index)
{
    std::cout << "node0向node" << node_index << "发送消息" << std::endl;
    Ptr<Packet> p = Create<Packet>(64);
    m_node0_socket->SendTo(p, 0, InetSocketAddress(dst, m_port));
}

void PingPongTraffic::NodeIRxCb(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        InetSocketAddress src_addr = InetSocketAddress::ConvertFrom(from);
        
        // 查找该包的跳数
        uint32_t hops = 0;
        if (m_uid_to_hops.find(packet->GetUid()) != m_uid_to_hops.end()) {
            hops = m_uid_to_hops[packet->GetUid()];
        }

        // 获取当前是哪个 node 收到了包
        Address local_addr;
        socket->GetSockName(local_addr);
        Ipv4Address my_ip = InetSocketAddress::ConvertFrom(local_addr).GetIpv4();
        uint32_t my_id = m_ip_to_node_id[my_ip];

        std::cout << "node" << my_id << "接收到了node0发送的消息，经过跳数为" << hops << std::endl;
        std::cout << "node" << my_id << "向node0发送消息" << std::endl;

        // 回复 node0
        Ptr<Packet> reply = Create<Packet>(64);
        socket->SendTo(reply, 0, src_addr);
    }
}

void PingPongTraffic::Node0RxCb(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        InetSocketAddress src_addr = InetSocketAddress::ConvertFrom(from);
        
        // 查找该包的跳数
        uint32_t hops = 0;
        if (m_uid_to_hops.find(packet->GetUid()) != m_uid_to_hops.end()) {
            hops = m_uid_to_hops[packet->GetUid()];
        }
        
        m_total_replies++;
        uint32_t sender_id = m_ip_to_node_id[src_addr.GetIpv4()];

        std::cout << "node0接收到了node" << sender_id << "发送的消息，经过跳数为" << hops
                  << "，进度" << m_total_replies << "/" << m_expected_replies << std::endl;
    }
}

void PingPongTraffic::IpLocalDeliverCb(std::string context, const Ipv4Header &ip_hdr, Ptr<const Packet> p, uint32_t ifs)
{
    m_uid_to_hops[p->GetUid()] = 64 - ip_hdr.GetTtl();
}

// 辅助函数：从 NS-3 的上下文字符串(如 "/NodeList/15/...") 中提取当前处理该包的 Node ID
static uint32_t GetNodeIdFromContext(std::string context) {
    std::size_t n1 = context.find("/NodeList/");
    if (n1 != std::string::npos) {
        n1 += 10;
        std::size_t n2 = context.find("/", n1);
        if (n2 != std::string::npos) {
            return std::stoi(context.substr(n1, n2 - n1));
        }
    }
    return 9999;
}

// 拦截【中间节点转发】动作
void PingPongTraffic::IpUnicastForwardCb(std::string context, const Ipv4Header &ip_hdr, Ptr<const Packet> p, uint32_t ifs)
{
    uint32_t current_node = GetNodeIdFromContext(context);
    
    // 只打印应用层发出的数据包（通过端口或 UID 过滤，避免打印海量的 AODV 协议心跳包）
    // UDP_CBR/PingPong 的 UID 通常较小或者连续，这里我们全部打印，你可以根据需要调整
    std::cout << "  [接力转发] node" << current_node 
              << " 正在转发包(UID:" << p->GetUid() << ")"
              << " | 源IP:" << ip_hdr.GetSource() 
              << " -> 目的IP:" << ip_hdr.GetDestination() 
              << " | 剩余寿命TTL:" << (uint32_t)ip_hdr.GetTtl() << std::endl;
}

// 拦截【网络层丢包】动作
void PingPongTraffic::IpDropCb(std::string context, const Ipv4Header &ip_hdr, Ptr<const Packet> p, Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t ifs)
{
    uint32_t current_node = GetNodeIdFromContext(context);
    
    // 丢弃原因解析 (NS-3 Ipv4L3Protocol::DropReason)
    std::string reason_str;
    switch(reason) {
        case Ipv4L3Protocol::DROP_TTL_EXPIRED: reason_str = "TTL耗尽(可能陷入死循环)"; break;
        case Ipv4L3Protocol::DROP_NO_ROUTE: reason_str = "找不到路由(遇到空洞或断连)"; break;
        case Ipv4L3Protocol::DROP_BAD_CHECKSUM: reason_str = "校验和错误"; break;
        case Ipv4L3Protocol::DROP_INTERFACE_DOWN: reason_str = "网卡关闭"; break;
        case Ipv4L3Protocol::DROP_ROUTE_ERROR: reason_str = "路由错误"; break;
        default: reason_str = "其他原因(码:" + std::to_string(reason) + ")"; break;
    }

    std::cout << "  [❌断点丢包!] node" << current_node 
              << " 丢弃了包(UID:" << p->GetUid() << ")"
              << " | 源IP:" << ip_hdr.GetSource() 
              << " -> 目的IP:" << ip_hdr.GetDestination() 
              << " | 原因: " << reason_str << std::endl;
}

