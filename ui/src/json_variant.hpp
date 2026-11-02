#pragma once

#include <QVariant>

#include "sirenproof/core/json_codec.hpp"

namespace sirenproof::ui {

// JSON from the service <-> the QVariant maps and lists QML works with.
QVariant to_variant(const core::Json& json);
core::Json from_variant(const QVariant& value);

}  // namespace sirenproof::ui
