#ifndef NETTRAFFIIC_H
#define NETTRAFFIIC_H

#include <vector>
#include <string>
#include "ns3/core-module.h"
#include "ns3/ip-l4-protocol.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-interface-container.h"
#include "utils/tracers.h"
#include "ns3/traffic-control-layer.h"

class NetTraffic;

class NetTraffic : public ns3::Object
{
protected:
  virtual NetTraffic* Clone() const = 0;
    uint64_t m_sindex;
    double m_total_time;
public:

  static ns3::TypeId GetTypeId (void)
  {
    static ns3::TypeId tid = ns3::TypeId ("NetTraffic")
      .SetParent<ns3::Object> ();
    return tid;
  }

  NetTraffic();
  virtual ~NetTraffic();
  NetTraffic& operator=(const NetTraffic&) = delete;
  NetTraffic(const NetTraffic&) = delete;

  virtual void SetSimulationTime(double t);
  virtual uint64_t GetStreamIndex() const;
  virtual double GetTotalSimTime() const;
  virtual uint32_t Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time) = 0;
  virtual int ConfigreTracing() = 0;

};

class PingTraffic : public NetTraffic
{
private:
  std::vector<ns3::Ptr<PingTracer> > m_ping_trace;
  virtual NetTraffic* Clone() const override;
public:
  static ns3::TypeId GetTypeId (void)
  {
    static ns3::TypeId tid = ns3::TypeId ("Ping")
      .SetParent<NetTraffic> ()
      .AddConstructor<PingTraffic> ();
    return tid;
  }

  PingTraffic();
  virtual ~PingTraffic();
  virtual uint32_t Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time) override;
  virtual int ConfigreTracing() override;
};


class UdpCbrTraffic : public NetTraffic
{
private:
  ns3::Ptr<PDRAndThroughputMetr> m_pckt_tracer;
  std::vector<std::pair<ns3::Ptr<ns3::Socket>, ns3::Ptr<ns3::Socket> > > m_pair_sockets;
  uint16_t m_pckt_size;
  double m_interval;
  double m_start;

  void GenerateTraffic(ns3::Ptr<ns3::Socket> socket);

  virtual NetTraffic* Clone() const override;
public:
  static ns3::TypeId GetTypeId (void)
  {
    static ns3::TypeId tid = ns3::TypeId ("UDP_CBR")
      .SetParent<NetTraffic> ()
      .AddConstructor<UdpCbrTraffic> ();
    return tid;
  }
  UdpCbrTraffic();
  virtual ~UdpCbrTraffic();
  virtual uint32_t Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time) override;
  virtual int ConfigreTracing() override;
  //to callbacks
  void RxCb(ns3::Ptr<ns3::Socket> socket);
};

class L3NodesDiscoverTraffic : public NetTraffic
{
private:
  ns3::Ptr<AdjTracer> m_adj_tracer;
  ns3::NetDeviceContainer m_devs;
  uint16_t m_pckt_size;
  double m_interval;
  double m_start;
  void GenerateTraffic(ns3::Ptr<ns3::Socket> socket);

  virtual NetTraffic* Clone() const override;

public:
  static ns3::TypeId GetTypeId (void)
  {
    static ns3::TypeId tid = ns3::TypeId ("L3ND")
      .SetParent<NetTraffic> ()
      .AddConstructor<L3NodesDiscoverTraffic> ();
    return tid;
  }

  static const uint16_t PROT_NUMBER;

  L3NodesDiscoverTraffic();
  virtual ~L3NodesDiscoverTraffic();
  virtual uint32_t Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time) override;
  virtual int ConfigreTracing() override;


  void ReceiveCb(ns3::Ptr<ns3::NetDevice> device, ns3::Ptr<const ns3::Packet> p, uint16_t protocol, const ns3::Address &from,
                 const ns3::Address &to, ns3::NetDevice::PacketType packetType );
  void TransmitCb(ns3::Ptr<ns3::NetDevice> dev, ns3::Ptr<ns3::TrafficControlLayer> tc, ns3::Ipv4InterfaceAddress ip_addr);
};

class UdpClientServerTraffic : public NetTraffic
{
private:
  uint16_t m_pckt_size;
  double m_interval;
  double m_start;

  virtual NetTraffic* Clone() const override;
public:
  static ns3::TypeId GetTypeId (void)
  {
    static ns3::TypeId tid = ns3::TypeId ("UDPClientServer")
      .SetParent<NetTraffic> ()
      .AddConstructor<UdpClientServerTraffic> ();
    return tid;
  }
  UdpClientServerTraffic();
  virtual ~UdpClientServerTraffic();
  virtual uint32_t Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time) override;
  virtual int ConfigreTracing() override;
};

class PingPongTraffic : public NetTraffic
{
private:
  uint16_t m_port;
  uint32_t m_total_replies;
  uint32_t m_expected_replies;
  ns3::Ptr<ns3::Socket> m_node0_socket;
  std::map<uint32_t, ns3::Ptr<ns3::Socket> > m_other_sockets;
  std::map<uint64_t, uint32_t> m_uid_to_hops;
  std::map<ns3::Ipv4Address, uint32_t> m_ip_to_node_id;

  void SendFromNode0(ns3::Ipv4Address dst, uint32_t node_index);
  void Node0RxCb(ns3::Ptr<ns3::Socket> socket);
  void NodeIRxCb(ns3::Ptr<ns3::Socket> socket);
  void IpLocalDeliverCb(const ns3::Ipv4Header &ip_hdr, ns3::Ptr<const ns3::Packet> p, uint32_t ifs);

  virtual NetTraffic* Clone() const override;

public:
  static ns3::TypeId GetTypeId (void);

  PingPongTraffic();
  virtual ~PingPongTraffic();
  virtual uint32_t Install(ns3::NodeContainer& nc, ns3::NetDeviceContainer& devs, ns3::Ipv4InterfaceContainer& ip_c, uint32_t stream_index, double start_time) override;
  virtual int ConfigreTracing() override;
};

void IpUnicastForwardCb(std::string context, const ns3::Ipv4Header &ip_hdr, ns3::Ptr<const ns3::Packet> p, uint32_t ifs);
  void IpDropCb(std::string context, const ns3::Ipv4Header &ip_hdr, ns3::Ptr<const ns3::Packet> p, ns3::Ipv4L3Protocol::DropReason reason, ns3::Ptr<ns3::Ipv4> ipv4, uint32_t ifs);
#endif // NETTRAFFIIC_H
