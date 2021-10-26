#pragma once

#include <render/vk/raii.hpp>

namespace sandbox::hal::render::avk
{
    class image_instance;

    class sampler_instance
    {
        friend class sampler_builder;

    public:
        sampler_instance() = default;
        operator vk::Sampler() const;

    private:
        avk::sampler m_sampler{};
    };


    class sampler_builder
    {
    public:
        sampler_builder& set_filtering(vk::Filter mag, vk::Filter min, vk::SamplerMipmapMode mip);
        sampler_builder& set_wrap(vk::SamplerAddressMode u, vk::SamplerAddressMode v, vk::SamplerAddressMode w);
        sampler_builder& set_anizatropy(float a);
        sampler_builder& set_compare(bool enabled, vk::CompareOp cmp = {});

        sampler_instance create(const image_instance&);

    private:
        vk::Filter m_min_filter{vk::Filter::eLinear};
        vk::Filter m_mag_filter{vk::Filter::eLinear};
        vk::SamplerMipmapMode m_mip_filter{vk::SamplerMipmapMode::eLinear};

        vk::SamplerAddressMode m_wrap_u{vk::SamplerAddressMode::eClampToEdge};
        vk::SamplerAddressMode m_wrap_v{vk::SamplerAddressMode::eClampToEdge};
        vk::SamplerAddressMode m_wrap_w{vk::SamplerAddressMode::eClampToEdge};


        float m_anizatropy{1.0};

        bool m_comapre_enabled{false};
        vk::CompareOp m_compare_op{};
    };
} // namespace sandbox::hal::render::avk