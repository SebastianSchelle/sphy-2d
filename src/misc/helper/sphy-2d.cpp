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
        if (callback)
        {
            if (!callback(typeId, version, des_))
            {
                return false;
            }
        }
    }
    return true;
}

namespace ctrl
{

void pidInit(PID* pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->prev_error = 0.0f;
    pid->integral = 0.0f;
}

float pidCompute(PID* pid, float dt, float setpoint, float measured)
{
    float error = setpoint - measured;
    float P = pid->kp * error;

    pid->integral += error * dt;
    float I = pid->ki * pid->integral;

    float derivative = (error - pid->prev_error) / dt;
    float D = pid->kd * derivative;

    float output = P + I + D;

    if (output > 1.0)
    {
        output = 1.0;
        pid->integral -= error * dt;  // Prevent integral windup
    }
    else if (output < -1.0)
    {
        output = -1.0;
        pid->integral -= error * dt;
    }

    pid->prev_error = error;
    return output;
}

void pdInit(PD* pd, float kp, float kd)
{
    pd->kp = kp;
    pd->kd = kd;
    pd->prev_error = 0.0f;
}

float pdComputeUnclamped(PD* pd, float dt, float error)
{
    float P = pd->kp * error;

    float derivative = (error - pd->prev_error) / dt;
    float D = pd->kd * derivative;

    float output = P + D;

    pd->prev_error = error;
    return output;
}

float pdCompute(PD* pd, float dt, float error)
{
    float output = pdComputeUnclamped(pd, dt, error);

    if (output > 1.0)
    {
        output = 1.0;
    }
    else if (output < -1.0)
    {
        output = -1.0;
    }

    return output;
}

}  // namespace ctrl

bool tryParseFloat(const string& text, float& outValue)
{
    if (text.empty())
    {
        return false;
    }
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0')
    {
        return false;
    }
    outValue = value;
    return true;
}

bool tryParseInt(const string& text, int& outValue)
{
    if (text.empty())
    {
        return false;
    }
    char* end = nullptr;
    const int value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0')
    {
        return false;
    }
    outValue = value;
    return true;
}

void floatToString(float value, string& outText, int precision)
{
    outText = fmt::format("{:.{}f}", value, precision);
}

void intToString(int value, string& outText)
{
    outText = fmt::format("{}", value);
}
