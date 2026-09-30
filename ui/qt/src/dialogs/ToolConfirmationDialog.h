#pragma once
#include <QDialog>
#include <QVariant>
namespace AtenaUi {
class ToolConfirmationDialog final : public QDialog {
    Q_OBJECT

public: explicit ToolConfirmationDialog(const QVariantMap &request,QWidget *parent=nullptr); bool allowedOnce() const;
private: bool m_allowed{false}; };
}
