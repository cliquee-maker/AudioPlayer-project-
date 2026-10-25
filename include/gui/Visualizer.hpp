#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

class IVisualSource;

class Visualizer : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Bars, Oscilloscope, Authentic };

    explicit Visualizer(IVisualSource* source, QWidget* parent = nullptr);

    Mode mode() const;
    void setMode(Mode mode);
    void cycleMode();
    QString modeName() const;

signals:
    void modeChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void paintBars(class QPainter& painter);
    void paintOscilloscope(class QPainter& painter);
    void paintAuthentic(class QPainter& painter);

    IVisualSource* source_;
    Mode mode_ = Mode::Bars;
    QVector<float> peaks_;
};
