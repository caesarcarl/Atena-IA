#include "StatusChip.h"
#include <QHBoxLayout>
#include <QLabel>

namespace AtenaUi {
StatusChip::StatusChip(QWidget *parent) : QFrame(parent), m_dot(new QLabel(this)), m_label(new QLabel(this)) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(6);
    m_dot->setFixedSize(8, 8);
    layout->addWidget(m_dot);
    layout->addWidget(m_label);
    setStatus(QStringLiteral("Core indisponível"), false);
}
void StatusChip::setStatus(const QString &label, bool positive) {
    m_label->setText(label);
    m_dot->setStyleSheet(QStringLiteral("border-radius:4px; background:%1;").arg(positive ? QStringLiteral("#268260") : QStringLiteral("#B87518")));
    setAccessibleName(QStringLiteral("Status: %1").arg(label));
}
}
