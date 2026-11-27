// Los programas que vienen con la app (Python, yt-dlp, ffmpeg, rsync, dbclient) y el entorno
// para ejecutarlos. Los ejecutables están en la carpeta de bibliotecas nativas de la app; sus
// bibliotecas y yt-dlp se descomprimen de entorno.tgz (en los assets del APK) la primera vez
// que se abre.

#pragma once

#include <QFile>
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
    Q_PROPERTY(QString versionYtDlp READ versionYtDlp NOTIFY versionYtDlpCambio)   // «2026.11.20» ("" si no se sabe)

public:
    explicit Herramientas(QObject *parent = nullptr);

    void preparar();
    bool listas() const { return preparadas; }
    bool preparando() const { return preparando_; }
    QString error() const { return error_; }
    QString versionYtDlp() const { return versionYtDlp_; }

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
    QString dropbearkey() const;            // sección Servidor: crea la clave del celular
    bool respaldoDisponible() const;

signals:
    void preparacionTerminada(bool bien, const QString &error);
    void estadoCambio();
    void versionYtDlpCambio();

private:
    void terminarPreparacion();
    void pasarTrozo();
    void crearEnlaces();
    QString entorno() const;
    QString marca() const;
    void consultarVersionYtDlp();

    QProcess descompresor;
    QFile paquete;      // entorno.tgz, mientras se descomprime
    bool preparadas = false;
    bool preparando_ = false;
    QString error_;
    QString versionYtDlp_;
};
