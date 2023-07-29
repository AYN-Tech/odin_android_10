// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "cdm_session_map.h"

#include <assert.h>

#include "cdm_session.h"
#include "log.h"

namespace wvcdm {

CdmSessionMap::~CdmSessionMap() {
  Terminate();
}

void CdmSessionMap::Terminate() {
  for (CdmIdToSessionMap::iterator i = sessions_.begin();
       i != sessions_.end(); ++i) {
    i->second->Close();
    i->second.reset();
  }
  sessions_.clear();
}

void CdmSessionMap::Add(const std::string& id, CdmSession* session) {
  sessions_[id].reset(session);
}

bool CdmSessionMap::CloseSession(const std::string& id) {
  std::shared_ptr<CdmSession> session;
  if (!FindSessionNoLock(id, &session)) {
    return false;
  }
  session->Close();
  sessions_.erase(id);
  return true;
}

bool CdmSessionMap::Exists(const std::string& id) {
  return sessions_.find(id) != sessions_.end();
}

bool CdmSessionMap::FindSession(const CdmSessionId& id,
                                std::shared_ptr<CdmSession>* session) {
  return FindSessionNoLock(id, session);
}

bool CdmSessionMap::FindSessionNoLock(const CdmSessionId& session_id,
                                      std::shared_ptr<CdmSession>* session) {
  CdmIdToSessionMap::iterator iter = sessions_.find(session_id);
  if (iter == sessions_.end()) {
    return false;
  }
  *session = iter->second;
  assert(session->get() != NULL);
  return true;
}

void CdmSessionMap::GetSessionList(CdmSessionList& sessions) {
  sessions.clear();
  for (CdmIdToSessionMap::iterator iter = sessions_.begin();
       iter != sessions_.end(); ++iter) {
    if (!(iter->second)->IsClosed()) {
      sessions.push_back(iter->second);
    }
  }
}

}  // namespace wvcdm
