#include "std-inc.hpp"

namespace tim
{

const extern boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));

}  // namespace tim

bool LOAD_OBJ(
    const std::string& path_,
    std::function<bool(uint32_t,
                       uint16_t,
                       bitsery::Deserializer<InputAdapter>&)> callback)
{
    if (!fs::exists(path_))
    {
        LG_E("File {} not found", path_);
        return false;
    }
    std::ifstream file(path_, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LG_E("Failed to open file {}", path_);
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data_;
    if (size > 0)
    {
        data_.resize(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(data_.data()), size))
        {
            LG_E("Failed to read file {}", path_);
            file.close();
            return false;
        }
    }
    file.close();

    bitsery::Deserializer<InputAdapter> des_(
        InputAdapter{data_.begin(), data_.size()});
    while (data_.size() - des_.adapter().currentReadPos() > 6)
    {
        uint32_t typeId = 0;
        des_.value4b(typeId);
        uint16_t version = 0;
        des_.value2b(version);
        if(callback)
        {
            if(!callback(typeId, version, des_))
            {
                return false;
            }
        }
    }
    return true;
}
