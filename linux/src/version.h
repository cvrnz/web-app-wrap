#pragma once

#include <QString>
#include <optional>

// Which WebAppWrap this is: the commit scripts/build built it from, with "-dirty" for
// uncommitted changes. An app in the list is rebuilt when this changes. Builds made without
// scripts/build have no version and rebuild every app every time.
namespace Version {
std::optional<QString> commit();
// "71471f3", "71471f3-dirty" or "development build".
QString describe(const std::optional<QString> &commit);
QString display();
}
