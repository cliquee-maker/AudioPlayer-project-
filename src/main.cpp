#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("Audio Player - Qt smoke");
    window.resize(640, 360);

    auto* central = new QWidget(&window);
    auto* layout = new QVBoxLayout(central);

    auto* label = new QLabel("Qt 6 Widgets smoke test", central);
    auto* button = new QPushButton("Click me", central);
    layout->addWidget(label);
    layout->addWidget(button);

    window.setCentralWidget(central);
    window.show();

    return app.exec();
}
