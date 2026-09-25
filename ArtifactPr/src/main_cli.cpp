
#include <QCoreApplication>

import ArtifactPr.CLI;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    return runArtifactPrCli(app.arguments());
}
