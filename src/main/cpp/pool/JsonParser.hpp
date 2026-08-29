#pragma once
// ============================================================
// TrialSpawnerFinder C++ JSON 解析器
//
// 逐位复刻 cn.trialfinder.sim.json.Json（26.2 语义）——极简只读 DOM。
// 足够解析模板池 pool JSON 与 structure/trial_chambers.json。
// 数字统一按 double 解析（int 访问向零截断，与 Java (int) 一致）。
// ============================================================

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace trialfinder {

class Json {
public:
    enum class Type : uint8_t { OBJECT, ARRAY, STRING, NUMBER, BOOL, NUL };

    Type type = Type::NUL;
    std::vector<std::pair<std::string, Json>> object;
    std::vector<Json> array;
    std::string str;
    double number = 0.0;
    bool boolean = false;

    // ---- 对象访问（对齐 Json.Object） ----
    bool has(const std::string& key) const;
    /** 返回成员节点指针；不存在返回 nullptr。 */
    const Json* get(const std::string& key) const;
    /** getString：不存在或非字符串返回 ""。 */
    std::string getString(const std::string& key) const;
    /** getInt：不存在或非数字返回 0（数字按 double 截断取整）。 */
    int getInt(const std::string& key) const;
    double getDouble(const std::string& key) const;
    /** getArray：不存在或非数组返回 nullptr。 */
    const Json* getArray(const std::string& key) const;

    // ---- 数组访问（对齐 Json.Array） ----
    std::size_t size() const { return array.size(); }
    const Json& at(std::size_t index) const { return array[index]; }

    // ---- 解析 ----
    static Json parse(const std::string& text);
    static Json parseBytes(const uint8_t* data, std::size_t len);
};

} // namespace trialfinder
