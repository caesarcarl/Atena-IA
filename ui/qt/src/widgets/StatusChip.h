#pragma once
#include <QFrame>
class QLabel;

namespace AtenaUi {
class StatusChip final : public QFrame {
    Q_OBJECT
public:
    explicit StatusChip(QWidget *parent = nullptr);
    void setStatus(const QString &label, bool positive);
private:
    QLabel *m_dot;
    QLabel *m_label;
};
}
