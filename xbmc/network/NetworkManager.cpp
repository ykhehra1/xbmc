/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "NetworkServices.h"
#include "NetworkManager.h"
#include "NullNetworkManager.h"
#include "Application.h"
#include "ApplicationMessenger.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/Key.h"
#include "linux/ConnmanNetworkManager.h"
#include "linux/PosixNetworkManager.h"
#include "windows/WinNetworkManager.h"
#include "utils/log.h"
#include "utils/RssManager.h"
#include "utils/RssReader.h"

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
CNetworkManager::CNetworkManager()
{
  m_timer = NULL;
  m_instance = NULL;
  m_state = NETWORK_CONNECTION_STATE_UNKNOWN;
}

CNetworkManager::~CNetworkManager()
{
  delete m_instance;
}

void CNetworkManager::Initialize()
{
#ifdef HAS_DBUS
//  if (CConnmanNetworkManager::HasConnman())
//    m_instance = new CConnmanNetworkManager();
#endif

#if defined(TARGET_POSIX)
  if (m_instance == NULL)
    m_instance = new CPosixNetworkManager();
#endif

#ifdef TARGET_WINDOWS
  if (m_instance == NULL)
    m_instance = new CWinNetworkManager();
#endif

  if (m_instance == NULL)
    m_instance = new CNullNetworkManager();

  m_defaultConnection = CConnectionPtr(new CNullConnection());
  OnConnectionListChange(m_instance->GetConnections());
}

bool CNetworkManager::PumpNetworkEvents()
{
#if defined(TARGET_ANDROID)
  if (!g_application.m_pPlayer)
  {
    if (!m_timer && !IsConnected())
    {
      CLog::Log(LOGDEBUG, "NetworkManager: not connected, bgn timout");
      m_timer = new CStopWatch();
      m_timer->StartZero();
    }
  }
  if (m_timer && m_timer->GetElapsedSeconds() > 15)
  {
    CLog::Log(LOGDEBUG, "NetworkManager: not connected, end timout");
    OnConnectionListChange(m_instance->GetConnections());
    delete m_timer, m_timer = NULL;
  }
#endif

  return m_instance->PumpNetworkEvents(this);
}

std::string CNetworkManager::GetDefaultConnectionName()
{
  if (m_defaultConnection)
    return m_defaultConnection->GetName();
  else
    return std::string("opps");
}

std::string CNetworkManager::GetDefaultConnectionAddress()
{
  if (m_defaultConnection)
    return m_defaultConnection->GetAddress();
  else
    return std::string("opps");
}

std::string CNetworkManager::GetDefaultConnectionNetmask()
{
  if (m_defaultConnection)
    return m_defaultConnection->GetNetmask();
  else
    return std::string("opps");
}

std::string CNetworkManager::GetDefaultConnectionMacAddress()
{
  if (m_defaultConnection)
    return m_defaultConnection->GetMacAddress();
  else
    return std::string("opps");
}

std::string CNetworkManager::GetDefaultConnectionGateway()
{
  if (m_defaultConnection)
    return m_defaultConnection->GetGateway();
  else
    return std::string("opps");
}

std::string CNetworkManager::GetDefaultConnectionNameServer()
{
  if (m_defaultConnection)
    return m_defaultConnection->GetNameServer();
  else
    return std::string("127.0.0.1");
}

ConnectionType CNetworkManager::GetDefaultConnectionType()
{
  if (m_defaultConnection)
    return m_defaultConnection->GetType();
  else
    return NETWORK_CONNECTION_TYPE_UNKNOWN;
}

IPConfigMethod CNetworkManager::GetDefaultConnectionMethod()
{
  if (m_defaultConnection)
    return m_defaultConnection->GetMethod();
  else
    return IP_CONFIG_DISABLED;
}

ConnectionState CNetworkManager::GetDefaultConnectionState()
{
  return m_state;
}

bool CNetworkManager::IsConnected()
{
  return GetDefaultConnectionState() == NETWORK_CONNECTION_STATE_CONNECTED;
}

bool CNetworkManager::IsAvailable(bool wait)
{
  return true;
}

bool CNetworkManager::CanManageConnections()
{
  return m_instance->CanManageConnections();
}

ConnectionList CNetworkManager::GetConnections()
{
  return m_connections;
}

void CNetworkManager::OnConnectionStateChange(ConnectionState state)
{
  ConnectionState oldState = m_state;
  m_state = state;

  if (m_state != oldState)
    CLog::Log(LOGDEBUG, "NetworkManager: State changed to %s", ConnectionStateToString(m_state));

  if (oldState != NETWORK_CONNECTION_STATE_CONNECTED && m_state == NETWORK_CONNECTION_STATE_CONNECTED)
    StartServices();
  else if (oldState == NETWORK_CONNECTION_STATE_CONNECTED && oldState != m_state)
    StopServices();
}

void CNetworkManager::OnConnectionChange(CConnectionPtr connection)
{
  if (connection->GetState() == NETWORK_CONNECTION_STATE_CONNECTED)
    m_defaultConnection = connection;

  if (g_windowManager.GetWindow(WINDOW_DIALOG_ACCESS_POINTS))
  {
    CAction action(ACTION_CONNECTIONS_REFRESH);
    CApplicationMessenger::Get().SendAction(action, WINDOW_DIALOG_ACCESS_POINTS);
  }
}

void CNetworkManager::OnConnectionListChange(ConnectionList list)
{
  m_connections = list;

  for (unsigned int i = 0; i < m_connections.size(); i++)
  {
    if (m_connections[i]->GetState() == NETWORK_CONNECTION_STATE_CONNECTED)
    {
      m_defaultConnection = m_connections[i];
      OnConnectionStateChange(NETWORK_CONNECTION_STATE_CONNECTED);
      break;
    }
  }

  if (g_windowManager.GetWindow(WINDOW_DIALOG_ACCESS_POINTS))
  {
    CAction action(ACTION_CONNECTIONS_REFRESH);
    CApplicationMessenger::Get().SendAction(action, WINDOW_DIALOG_ACCESS_POINTS);
  }
}

void CNetworkManager::NetworkMessage(EMESSAGE message, int param)
{
  switch( message )
  {
    case SERVICES_UP:
    {
      StartServices();
    }
    break;
    case SERVICES_DOWN:
    {
      StopServices();
    }
    break;
  }
}

bool CNetworkManager::WakeOnLan(const char *mac)
{
  return m_instance->SendWakeOnLan(mac);
}

void CNetworkManager::StartServices()
{
  CLog::Log(LOGDEBUG, "%s - Starting network services",__FUNCTION__);

  // TODO: fix properly
  system("/etc/init.d/S49ntp restart");

  CNetworkServices::Get().Start();
}

void CNetworkManager::StopServices()
{
  CLog::Log(LOGDEBUG, "%s - Signaling network services to stop",__FUNCTION__);
  CNetworkServices::Get().Stop(false); // tell network services to stop, but don't wait for them yet
  CLog::Log(LOGDEBUG, "%s - Waiting for network services to stop",__FUNCTION__);
  CNetworkServices::Get().Stop(true); // wait for network services to stop
}

const char *CNetworkManager::ConnectionStateToString(ConnectionState state)
{
  switch (state)
  {
    case NETWORK_CONNECTION_STATE_FAILURE:
      return "failure";
    case NETWORK_CONNECTION_STATE_DISCONNECTED:
      return "disconnect";
    case NETWORK_CONNECTION_STATE_CONNECTING:
      return "connecting";
    case NETWORK_CONNECTION_STATE_CONNECTED:
      return "connected";
    case NETWORK_CONNECTION_STATE_UNKNOWN:
    default:
      return "unknown";
  }
}

//creates, binds and listens a tcp socket on the desired port. Set bindLocal to
//true to bind to localhost only. The socket will listen over ipv6 if possible
//and fall back to ipv4 if ipv6 is not available on the platform.
int CreateTCPServerSocket(const int port, const bool bindLocal, const int backlog, const char *callerName)
{
  struct sockaddr_storage addr;
  int    sock = -1;

#ifdef WINSOCK_VERSION
  int yes = 1;
  int no = 0;
#else
  unsigned int yes = 1;
  unsigned int no = 0;
#endif
  
  // first try ipv6
  if ((sock = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) >= 0)
  {
    // in case we're on ipv6, make sure the socket is dual stacked
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no)) < 0)
    {
#ifdef _MSC_VER
      CStdString sock_err = CWIN32Util::WUSysMsg(WSAGetLastError());
#else
      CStdString sock_err = strerror(errno);
#endif
      CLog::Log(LOGWARNING, "%s Server: Only IPv6 supported (%s)", callerName, sock_err.c_str());
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    struct sockaddr_in6 *s6;
    memset(&addr, 0, sizeof(addr));
    addr.ss_family = AF_INET6;
    s6 = (struct sockaddr_in6 *) &addr;
    s6->sin6_port = htons(port);
    if (bindLocal)
      s6->sin6_addr = in6addr_loopback;
    else
      s6->sin6_addr = in6addr_any;

    if (bind( sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in6)) < 0)
    {
      closesocket(sock);
      sock = -1;
      CLog::Log(LOGDEBUG, "%s Server: Failed to bind ipv6 serversocket, trying ipv4", callerName);
    }
  }
  
  // ipv4 fallback
  if (sock < 0 && (sock = socket(PF_INET, SOCK_STREAM, 0)) >= 0)
  {
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    struct sockaddr_in  *s4;
    memset(&addr, 0, sizeof(addr));
    addr.ss_family = AF_INET;
    s4 = (struct sockaddr_in *) &addr;
    s4->sin_port = htons(port);
    if (bindLocal)
      s4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else
      s4->sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind( sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0)
    {
      closesocket(sock);
      CLog::Log(LOGERROR, "%s Server: Failed to bind ipv4 serversocket", callerName);
      return INVALID_SOCKET;
    }
  }
  else if (sock < 0)
  {
    CLog::Log(LOGERROR, "%s Server: Failed to create serversocket", callerName);
    return INVALID_SOCKET;
  }

  if (listen(sock, backlog) < 0)
  {
    closesocket(sock);
    CLog::Log(LOGERROR, "%s Server: Failed to set listen", callerName);
    return INVALID_SOCKET;
  }

  return sock;
}

