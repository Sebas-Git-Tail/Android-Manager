// Los programas que vienen con la app (Python, yt-dlp, ffmpeg, rsync, dbclient) y el entorno
// para ejecutarlos. Los ejecutables están en la carpeta de bibliotecas nativas de la app; sus
// bibliotecas y yt-dlp se descomprimen de libentorno.tar.so la primera vez que se abre.

#pragma once

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

class Herramientas : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listas READ listas NOTIFY estadoCambio)
    Q_PROPERTY(bool preparando READ preparando NOTIFY estadoCambio)
    Q_PROPERTY(QString error READ error NOTIFY estadoCambio)        // por qué no se pudieron preparar

public:
    explicit Herramientas(QObject *parent = nullptr);

    void preparar();
    bool listas() const { return preparadas; }
    bool preparando() const { return preparando_; }
    QString error() const { return error_; }

    QProcessEnvironment ambiente() const;
    QString temporal() const;

    // yt-dlp
    QString python() const;
    QStringList argumentosYtDlp() const;    // yt-dlp, ffmpeg y QuickJS (para los desafíos de YouTube)
    bool ytDlpDesactualizado() const;       // se actualiza una vez por día
    void marcarYtDlpActualizado();

    // respaldo
    QString rsync() const;
    QString dbclient() const;
    bool respaldoDisponible() const;

signals:
    void preparacionTerminada(bool bien, const QString &error);
    void estadoCambio();

private:
    void terminarPreparacion();
    void crearEnlaces();
    QString entorno() const;
    QString marca() const;

    QProcess descompresor;
    bool preparadas = false;
    bool preparando_ = false;
    QString error_;
};
