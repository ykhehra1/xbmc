#pragma once
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

#include "IKeyringManager.h"
#include "MemoryKeyringManager.h"

class CKeyringManager
{
public:
  CKeyringManager();
  ~CKeyringManager();

  void Initialize();

  /*!
   \brief Find a stored secret

   \param keyring to look for secret within
   \param key of secret to look for
   \param secret will be filled with secret if found
   \return true if found, false if not
   */
  bool FindSecret(const std::string &keyring, const std::string &key, CVariant &secret);

  /*!
   \brief Erase a stored secret

   \param keyring to look for secret within
   \param key of secret to look for
   \return true if secret was erased i.e. not existant in keyring, false if failed to do so
   */
  bool EraseSecret(const std::string &keyring, const std::string &key);

  /*!
   \brief Store a secret

   \param keyring to store secret within
   \param key of secret to store
   \param secret to be stored
   \param persistant true if this secret should be stored to persistant storage
   \return true if stored, false if not
   */
  bool StoreSecret(const std::string &keyring, const std::string &key, const CVariant &secret, bool persistant = true);

private:
  CMemoryKeyringManager   m_temporaryKeyringManager;
  IKeyringManager        *m_persistantKeyringManager;
};
