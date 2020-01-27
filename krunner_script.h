#ifndef KRUNNER_BRIDGE_KRUNNER_SCRIPT_H
#define KRUNNER_BRIDGE_KRUNNER_SCRIPT_H

#include <QRegularExpression>

struct KRunnerScript {

    QRegularExpression matchRegex = QRegularExpression(".*");
    QString launchCommand = "python3";
    QString filePath;
    bool initialize = true;
    bool prepare = false;
    bool teardown = false;
    bool runDetatched = true;
};

#endif //KRUNNER_BRIDGE_KRUNNER_SCRIPT_H
