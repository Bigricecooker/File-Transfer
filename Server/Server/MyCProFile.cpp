#include "MyCProFile.h"

static const std::string_view emptyView;

MyCProFile::MyCProFile(std::string&& data) :m_data(std::move(data))
{
	Parse();
}


MyCProFile::MyCProFile(std::string_view data) :m_data(data)
{
	Parse();
}

MyCProFile::MyCProFile(const MyCProFile& other) : m_data(other.m_data)
{
	Parse();
}

MyCProFile& MyCProFile::operator=(const MyCProFile& other)
{
	if (this != &other)
	{
		m_data = other.m_data;
		Reset();
		Parse();
	}
	return *this;
}

MyCProFile::MyCProFile(MyCProFile&& other) : m_data(std::move(other.m_data))
{
	other.Reset();
	Parse();
}

MyCProFile& MyCProFile::operator=(MyCProFile&& other)
{
	if (this != &other)
	{
		m_data = std::move(other.m_data);
		Reset();
		other.Reset();
		Parse();
	}
	return *this;
}

void MyCProFile::Reset()
{
	this->m_mapSections.clear();
	this->m_vctSections.clear();
}

const std::string_view MyCProFile::GetValue(const std::string& strSection, const std::string& strKey) const
{
	auto section = m_mapSections.find(strSection);
	if (section != m_mapSections.end())
	{
		auto it = section->second.find(strKey);
		if (it != section->second.end())
			return it->second;
	}
	return emptyView;
}

const std::vector<std::string_view>& MyCProFile::GetSections() const
{
	return m_vctSections;
}

std::vector<std::string_view> MyCProFile::GetKeysBySection(const std::string& strSection) const
{
	std::vector<std::string_view> Keys;
	auto section = m_mapSections.find(strSection);
	if (section != m_mapSections.end())
	{
		for (const auto& kv : section->second)
		{
			if (!kv.first.empty())
				Keys.push_back(kv.first);
		}
	}
	return Keys;
}

std::map<std::string, std::string> MyCProFile::GetKeyValueMapBySection(const std::string& strSection) const
{

	std::map<std::string, std::string> result;
	auto section = m_mapSections.find(strSection);
	if (section != m_mapSections.end())
	{
		for (const auto& kv : section->second)
		{
			if (!kv.first.empty())  // 过滤空键
			{
				// 使用 emplace 直接构造，避免临时变量
				result.emplace(kv.first, kv.second);
			}
		}
	}
	return result;
}

std::vector<std::string_view> MyCProFile::GetStringGroup(const std::string& strSection, const std::string& strKey) const
{
	std::vector<std::string_view> result;
	auto section = m_mapSections.find(strSection);
	if (section != m_mapSections.end())
	{
		auto it = section->second.find(strKey);
		if (it != section->second.end())
		{
			std::string_view valueView = it->second;
			size_t start = 0;
			while (start < valueView.size())
			{
				size_t end = valueView.find(',', start);
				if (end == std::string_view::npos)
					end = valueView.size();
				result.push_back(valueView.substr(start, end - start));
				start = end + 1;
			}
		}
	}
	return result;
}

bool MyCProFile::LoadFromFile(const std::string& filePath)
{
	Reset();
	std::ifstream file;
	file.open(filePath, std::ios::in | std::ios::binary);// 默认以文本方式打开，二进制模式下不处理换行符，保持原始数据

	if (!file.is_open())
	{
		return false;
	}

	std::ostringstream buffer;
	buffer << file.rdbuf();
	m_data = buffer.str();

	Parse();

	file.close();
	return true;
}

void MyCProFile::Set(std::string_view data) {
	this->m_data = data;
	Reset();
	Parse();
}

void MyCProFile::Set(std::string&& strCfg) {
	this->m_data = std::move(strCfg);
	Reset();
	Parse();
}

void MyCProFile::Parse() {
	if (m_data.empty()) {
		return;
	}

	// 总串
	std::string_view dataView(m_data);
	std::string_view currentSection; //当前节

	// 按行解析
	size_t pos = 0;

	size_t dataSize = dataView.size();

	while (pos < dataSize)
	{
		size_t nextPos = dataView.find("\r\n", pos);

		std::string_view line;
		if (nextPos == std::string_view::npos)
		{
			line = dataView.substr(pos);
			pos = dataSize;
		}
		else
		{
			line = dataView.substr(pos, nextPos - pos);
			pos = nextPos + 2;
		}

		// 跳过空行
		if (line.empty()) continue;

		// 获取去掉其他字符内容的行
		size_t validLen = line.find_last_not_of("\r\n");
		if (validLen == std::string_view::npos) continue;
		line = line.substr(0, validLen + 1); // 去除行尾的\r\n
		if (line.empty()) continue;

		// 解析节
		if (line.front() == '[' && line.back() == ']') {
			std::string_view sectionView = line.substr(1, line.length() - 2);
			currentSection = sectionView;
			m_vctSections.push_back(currentSection);
		}
		// 解析 Key=Value
		else
		{
			auto ePos = line.find('=');
			if (ePos != std::string_view::npos)
			{
				std::string_view keyView = line.substr(0, ePos);
				std::string_view valueView = line.substr(ePos + 1);

				m_mapSections[currentSection][keyView] = valueView;
			}
		}
	}
}
