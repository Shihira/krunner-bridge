#ifndef KRUNNER_BRIDGE
#define KRUNNER_BRIDGE

#include <QtCore/QProcess>
#include <KF5/KRunner/krunner/abstractrunner.h>

class KRunnerBridge : public Plasma::AbstractRunner {
    Q_OBJECT

    QString script;

public:
    KRunnerBridge(QObject* parent, const QVariantList& args);

    void match(Plasma::RunnerContext&);
    void run(const Plasma::RunnerContext&, const Plasma::QueryMatch&);
};

#endif // KRUNNER_BRIDGE
