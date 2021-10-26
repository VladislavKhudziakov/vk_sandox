#include "sampler.hpp"

#include <render/vk/resources/image.hpp>

using namespace sandbox::hal::render;


avk::sampler_instance::operator vk::Sampler() const
{
    return m_sampler.as<vk::Sampler>();
}


avk::sampler_builder& avk::sampler_builder::set_filtering(vk::Filter mag, vk::Filter min, vk::SamplerMipmapMode mip)
{
    m_mag_filter = mag;
    m_min_filter = min;
    m_mip_filter = mip;

    return *this;
}


avk::sampler_builder& avk::sampler_builder::set_wrap(vk::SamplerAddressMode u, vk::SamplerAddressMode v, vk::SamplerAddressMode w)
{ 
    m_wrap_u = u;
    m_wrap_v = v;
    m_wrap_w = w;

    return *this;
}


avk::sampler_builder& avk::sampler_builder::set_anizatropy(float a)
{
    m_anizatropy = a;

    return *this;
}


avk::sampler_builder& avk::sampler_builder::set_compare(bool enabled, vk::CompareOp cmp)
{
    m_comapre_enabled = enabled;
    m_compare_op = cmp;

    return *this;
}


avk::sampler_instance sandbox::hal::render::avk::sampler_builder::create(const image_instance& image)
{
    sampler_instance result{};

    auto limits = avk::context::gpu()->getProperties().limits;
    
    vk::SamplerCreateInfo sampler_info{
        .magFilter = m_min_filter,
        .minFilter = m_mag_filter,
        .mipmapMode = m_mip_filter,

        .addressModeU = m_wrap_u,
        .addressModeV = m_wrap_v,
        .addressModeW = m_wrap_w,
        .mipLodBias = 0,

        .anisotropyEnable = m_anizatropy <= 1.0,
        .maxAnisotropy = std::clamp(m_anizatropy, 1.0f, limits.maxSamplerAnisotropy),
        
        .compareEnable = m_comapre_enabled,
        .compareOp = m_compare_op,

        .minLod = 0,
        .maxLod = static_cast<float>(image.get_mips_levels() - 1),

        .borderColor = vk::BorderColor::eFloatOpaqueBlack,
        .unnormalizedCoordinates = VK_FALSE
    };

    result.m_sampler = avk::create_sampler(avk::context::device()->createSampler(sampler_info));

    return result;
}
