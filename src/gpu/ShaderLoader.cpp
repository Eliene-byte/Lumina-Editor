#include "ShaderLoader.h"

#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace lmn::gpu {
namespace {

QMutex& cacheMutex() {
    static QMutex m;
    return m;
}

std::unordered_map<std::string, QString>& sourceCache() {
    static std::unordered_map<std::string, QString> cache;
    return cache;
}

// Resolve includes recursivamente. A profundidade e limitada para um include
// cycico nao travar o processo.
QString resolveInto(const QString& resourcePath,
                    std::unordered_map<std::string, QString>& cache, int depth) {
    if (depth > 8) return {};

    const QByteArray key = resourcePath.toUtf8();
    const auto it = cache.find(key.constData());
    if (it != cache.end()) return it->second;

    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};

    QString out;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("#include"))) {
            // Aceita #include "x.glsl" e #include <x.glsl>. Todos os includes
            // sao resolvidos a partir de ":/shaders/", que e onde os shaders
            // sao compilados no recurso.
            const int open = trimmed.indexOf(QLatin1Char('"'));
            const int openAlt = trimmed.indexOf(QLatin1Char('<'));
            int start = (open >= 0) ? open : openAlt;
            if (start >= 0) {
                const QChar closer = (open >= 0) ? QLatin1Char('"') : QLatin1Char('>');
                const int end = trimmed.indexOf(closer, start + 1);
                if (end > start) {
                    const QString name = trimmed.mid(start + 1, end - start - 1);
                    out += resolveInto(QStringLiteral(":/shaders/") + name, cache, depth + 1);
                    out += QLatin1Char('\n');
                    continue;
                }
            }
        }
        out += line;
        out += QLatin1Char('\n');
    }

    cache.emplace(key.constData(), out);
    return out;
}

}  // namespace

QString ShaderLoader::resolve(const QString& resourcePath,
                              std::unordered_map<std::string, QString>* cache) {
    std::unordered_map<std::string, QString> local;
    return resolveInto(resourcePath, cache ? *cache : local, 0);
}

QString ShaderLoader::loadFragment(const QString& resourcePath) {
    QMutexLocker lock(&cacheMutex());
    return resolve(resourcePath, &sourceCache());
}

void ShaderLoader::clearCache() {
    QMutexLocker lock(&cacheMutex());
    sourceCache().clear();
}

}  // namespace lmn::gpu
