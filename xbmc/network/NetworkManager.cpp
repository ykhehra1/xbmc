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
