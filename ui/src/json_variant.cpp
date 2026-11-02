#include "json_variant.hpp"

#include <QJSValue>
#include <QVariantList>
#include <QVariantMap>

namespace sirenproof::ui {

QVariant to_variant(const core::Json& json) {
    switch (json.type()) {
        case core::Json::value_t::boolean: return json.get<bool>();
        case core::Json::value_t::number_integer:
        case core::Json::value_t::number_unsigned: return QVariant::fromValue(json.get<qint64>());
        case core::Json::value_t::number_float: return json.get<double>();
        case core::Json::value_t::string: return QString::fromStdString(json.get<std::string>());
        case core::Json::value_t::array: {
            QVariantList list;
            for (const auto& item : json) list.push_back(to_variant(item));
            return list;
        }
        case core::Json::value_t::object: {
            QVariantMap map;
            for (const auto& item : json.items()) map.insert(QString::fromStdString(item.key()), to_variant(item.value()));
            return map;
        }
        default: return {};
    }
}

core::Json from_variant(const QVariant& value) {
    if (value.metaType() == QMetaType::fromType<QJSValue>()) return from_variant(value.value<QJSValue>().toVariant());
    switch (value.metaType().id()) {
        case QMetaType::UnknownType:
        case QMetaType::Nullptr: return nullptr;
        case QMetaType::Bool: return value.toBool();
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong: return value.toLongLong();
        case QMetaType::Double:
        case QMetaType::Float: return value.toDouble();
        case QMetaType::QVariantList:
        case QMetaType::QStringList: {
            core::Json list = core::Json::array();
            for (const QVariant& item : value.toList()) list.push_back(from_variant(item));
            return list;
        }
        case QMetaType::QVariantMap: {
            core::Json object = core::Json::object();
            const QVariantMap map = value.toMap();
            for (auto it = map.cbegin(); it != map.cend(); ++it) object[it.key().toStdString()] = from_variant(it.value());
            return object;
        }
        default: return value.toString().toStdString();
    }
}

}  // namespace sirenproof::ui
