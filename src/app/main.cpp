/**
 * Copyright 2023 <Hardworking Bee>
 */

#include <QApplication>

#include "../cheater/cheater.h"

using cheater::Cheater;

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  Cheater cheater;
  cheater.show();

  return app.exec();
}
