#pragma once

#include <QStringList>

// The command line: `waw build`, `waw batch` and the rest. None of it needs a display.
namespace Cli {
// Whether `arguments` (without the program name) is a command-line command rather than the
// window or an app.
bool handles(const QStringList &arguments);
// Runs it and returns the exit code: 0 on success, 1 on failure with errors on stderr.
int run(const QStringList &arguments);
QString helpText();
}
