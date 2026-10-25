#pragma once

#include <QVector>
#include <QWidget>

class IVisualSource;

class Visualizer : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Bars };

    explicit Visualizer(IVisualSource* source, QWidget* parent = nullptr);

    Mode mode() const;
    void setMode(Mode mode);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void paintBars(class QPainter& painter);

    IVisualSource* source_;
    Mode mode_ = Mode::Bars;
    QVector<float> peaks_;
};
