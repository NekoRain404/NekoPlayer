#pragma once

#include "sessionstate.h"

class SessionStore final {
  public:
    explicit SessionStore(QString filePath);
    [[nodiscard]] SessionLoadResult load() const;
    [[nodiscard]] QString save(const SessionState &state) const;

  private:
    QString filePath_;
};
