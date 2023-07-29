// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_CDM_SESSION_MAP_H_
#define WVCDM_CORE_CDM_SESSION_MAP_H_

#include <list>
#include <memory>
#include <string>

#include "cdm_session.h"
#include "disallow_copy_and_assign.h"
#include "wv_cdm_types.h"

namespace wvcdm {

typedef std::list<std::shared_ptr<CdmSession> > CdmSessionList;

// TODO(rfrias): Concurrency protection for this class has moved to CdmEngine.
// Add it back when locks to control access to session usage and destruction
// are introduced.
class CdmSessionMap {
 public:
  CdmSessionMap() {}
  virtual ~CdmSessionMap();

  // Use |Terminate| rather than relying on the destructor to release
  // resources, as it can be protected by locks.
  void Terminate();

  void Add(const std::string& id, CdmSession* session);

  bool CloseSession(const std::string& id);

  bool Exists(const std::string& id);

  size_t Size() const { return sessions_.size(); }

  bool FindSession(const CdmSessionId& id,
                   std::shared_ptr<CdmSession>* session);

  void GetSessionList(CdmSessionList& sessions);

 private:
  typedef std::map<CdmSessionId, std::shared_ptr<CdmSession> >
      CdmIdToSessionMap;

  bool FindSessionNoLock(const CdmSessionId& session_id,
                         std::shared_ptr<CdmSession>* session);

  CdmIdToSessionMap sessions_;

  CORE_DISALLOW_COPY_AND_ASSIGN(CdmSessionMap);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_CDM_SESSION_MAP_H_
