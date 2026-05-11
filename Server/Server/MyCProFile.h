#ifndef MY_CPRO_FILE_H
#define MY_CPRO_FILE_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <sstream>
#include <unordered_map>

// string_view优化后的解析类
class MyCProFile final
{
public:
	MyCProFile() = default;
	~MyCProFile() = default;

	MyCProFile(std::string&& data);
	MyCProFile(std::string_view data);

	// 自定义拷贝构造和拷贝赋值
	MyCProFile(const MyCProFile& other);
	MyCProFile& operator=(const MyCProFile& other);

	// 自定义移动构造和移动赋值
	MyCProFile(MyCProFile&& other);
	MyCProFile& operator=(MyCProFile&& other);


	/**********************************************************
	*    功  能：读取文件内容并解析
	*    参  数：
	*    返回值：bool 读取和解析是否成功
	**********************************************************/
	bool LoadFromFile(const std::string& filePath);

	/**********************************************************
	*    功  能：设置配置字符串
	*    参  数：strCfg 标准配置库字符串
	*    返回值：无
	**********************************************************/
	void Set(std::string_view data);
	void Set(std::string&& strCfg);

	/**********************************************************
	*    功  能：解析配置字符串
	*    参  数：无
	*    返回值：无
	**********************************************************/
	void Parse();

	/**********************************************************
	*    功  能：重置配置信息
	*    参  数：无
	*    返回值：无
	**********************************************************/
	void Reset();

	/**********************************************************
	*    功  能：根据Section和Key获取配置项的值
	*    参  数：strSection 节名称，strKey 主键名称
	*    返回值：string 指定的配置项内容
	**********************************************************/
	const std::string_view GetValue(const std::string& strSection, const std::string& strKey) const;

	/**********************************************************
	*    功  能：获取当前配置项中的所有节名称
	*    参  数：无
	*    返回值：vector<string> 配置项中的所有Section名称
	**********************************************************/
	const std::vector<std::string_view>& GetSections() const;

	/**********************************************************
	*    功  能：获取指定节下的所有Key名称
	*    参  数：strSection 节名称
	*    返回值：vector<string> 指定节下的所有Key名称
	**********************************************************/
	std::vector<std::string_view> GetKeysBySection(const std::string& strSection) const;

	/**********************************************************
	*    功  能：获取指定节下的所有键值对
	*    参  数：strSection 节名称
	*    返回值：std::unordered_map<std::string, std::string> 指定节下的所有键值对
	**********************************************************/
	std::map<std::string, std::string> GetKeyValueMapBySection(const std::string& strSection) const;

	/**********************************************************
	*    功  能：根据Section和Key获取配置项的值，并转为vector<string>
	*    参  数：strSection 节名称，strKey 主键名称
	*    返回值：vector<string> 指定的配置项内容
	**********************************************************/
	std::vector<std::string_view> GetStringGroup(const std::string& strSection, const std::string& strKey) const;
private:
	std::map<std::string_view, std::map<std::string_view, std::string_view>> m_mapSections;
	std::vector<std::string_view> m_vctSections;
	std::string m_data;
};

#endif // MY_CPRO_FILE_H