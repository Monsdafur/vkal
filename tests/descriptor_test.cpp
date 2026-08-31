#include "../vkal/src/descriptor.cpp"
#include "../vkal/src/descriptor_set.cpp"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

namespace vkal {

struct TestObjects {
    InstancePtr vkal_instance;
    DevicePtr vkal_device;
};

TestObjects create_test_objects() {
    SDL_Init(SDL_INIT_VIDEO);

    InstancePtr vkal_instance = instance_ptr();
    DevicePtr vkal_device = device_ptr(vkal::DeviceParams{
        .vkal_instance = *vkal_instance,
        .device_extensions = {vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
                              vk::KHRSynchronization2ExtensionName}});
    return TestObjects{
        .vkal_instance = std::move(vkal_instance),
        .vkal_device = std::move(vkal_device),
    };
}

TEST(DescriptorTest, CollectPoolSize) {
    TestObjects o = create_test_objects();

    DescriptorLayoutPtr l0 = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 2),
        }});
    DescriptorLayoutPtr l1 = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eSampledImage, 16),
        }});

    std::vector<vk::DescriptorPoolSize> pool_sizes = collect_descriptor_pool_sizes({*l0, *l1});
    ASSERT_EQ(pool_sizes.size(), 3);

    ASSERT_EQ(pool_sizes[0].type, vk::DescriptorType::eUniformBuffer);
    ASSERT_EQ(pool_sizes[0].descriptorCount, 3);

    ASSERT_EQ(pool_sizes[1].type, vk::DescriptorType::eStorageBuffer);
    ASSERT_EQ(pool_sizes[1].descriptorCount, 2);

    ASSERT_EQ(pool_sizes[2].type, vk::DescriptorType::eSampledImage);
    ASSERT_EQ(pool_sizes[2].descriptorCount, 16);

    SDL_Quit();
}

TEST(DescriptorTest, MatchPoolSize) {
    std::vector<vk::DescriptorPoolSize> lps = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 2),
    };
    std::vector<vk::DescriptorPoolSize> ps = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 3),
        vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 7),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 8),
        vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 2),
    };
    std::vector<size_t> psi = match_descriptor_pool_sizes(lps, ps);

    ASSERT_EQ(psi.size(), lps.size());

    ASSERT_EQ(psi.size(), 2);
    ASSERT_EQ(psi[0], 0);
    ASSERT_EQ(psi[1], 2);
}

TEST(DescriptorTest, MatchPoolSizeFail) {
    std::vector<vk::DescriptorPoolSize> lps = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 2),
    };
    std::vector<vk::DescriptorPoolSize> ps = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 3),
        vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 7),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 8),
        vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 2),
    };

    std::vector<size_t> psi = match_descriptor_pool_sizes(lps, ps);

    ASSERT_NE(psi.size(), lps.size());
}

TEST(DescriptorTest, GetPool) {
    TestObjects o = create_test_objects();

    DescriptorLayoutPtr l = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1),
        }});

    DescriptorPtr ds = descriptor_ptr(DescriptorParams{
        .vkal_device = *o.vkal_device,
        .max_sets = 10,
        .pool_size = 10,
    });

    DescriptorPoolInfo pi = ds->get_pool({*l});

    ASSERT_EQ(ds->pools.size(), 1);

    ASSERT_EQ(pi.pool.remaining_sets, 9);
    ASSERT_EQ(pi.pool_size_indices.size(), 2);
    ASSERT_EQ(pi.pool_size_indices[0], 0);
    ASSERT_EQ(pi.pool_size_indices[1], 1);

    ASSERT_EQ(pi.pool.pool_sizes[0].type, vk::DescriptorType::eUniformBuffer);
    ASSERT_EQ(pi.pool.pool_sizes[0].descriptorCount, 8);

    ASSERT_EQ(pi.pool.pool_sizes[1].type, vk::DescriptorType::eStorageBuffer);
    ASSERT_EQ(pi.pool.pool_sizes[1].descriptorCount, 9);

    SDL_Quit();
}

TEST(DescriptorTest, GetPoolFail) {
    TestObjects o = create_test_objects();

    DescriptorLayoutPtr l = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 11),
        }});

    DescriptorPtr ds = descriptor_ptr(DescriptorParams{
        .vkal_device = *o.vkal_device,
        .max_sets = 10,
        .pool_size = 10,
    });

    EXPECT_THROW(ds->get_pool({*l}), std::runtime_error);

    SDL_Quit();
}

TEST(DescriptorTest, GetMultiplePools) {
    TestObjects o = create_test_objects();

    DescriptorLayoutPtr l0 = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageImage, 1),
        }});
    DescriptorLayoutPtr l1 = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 2),
        }});

    DescriptorPtr ds = descriptor_ptr(DescriptorParams{
        .vkal_device = *o.vkal_device,
        .max_sets = 10,
        .pool_size = 10,
    });

    DescriptorPoolInfo pi0 = ds->get_pool({*l0});
    DescriptorPoolInfo pi1 = ds->get_pool({*l1});

    ASSERT_EQ(ds->pools.back()->remaining_sets, 8);

    ASSERT_EQ(ds->pools.size(), 1);
    ASSERT_EQ(ds->pools.back()->pool_sizes.size(), 2);

    ASSERT_EQ(ds->pools.back()->pool_sizes[0].type, vk::DescriptorType::eUniformBuffer);
    ASSERT_EQ(ds->pools.back()->pool_sizes[0].descriptorCount, 8);

    ASSERT_EQ(ds->pools.back()->pool_sizes[1].type, vk::DescriptorType::eStorageImage);
    ASSERT_EQ(ds->pools.back()->pool_sizes[1].descriptorCount, 7);

    // Checking pool infos
    ASSERT_EQ(pi0.pool_size_indices.size(), 2);
    ASSERT_EQ(pi0.pool_size_indices[0], 0);
    ASSERT_EQ(pi0.pool_size_indices[1], 1);

    ASSERT_EQ(pi1.pool_size_indices.size(), 1);
    ASSERT_EQ(pi1.pool_size_indices[0], 1);

    SDL_Quit();
}

TEST(DescriptorTest, GetMultipleUnfit) {
    TestObjects o = create_test_objects();

    DescriptorLayoutPtr l0 = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageImage, 2),
        }});
    DescriptorLayoutPtr l1 = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1),
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eSampler, 1),
            vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eSampledImage, 4),
        }});

    DescriptorPtr ds = descriptor_ptr(DescriptorParams{
        .vkal_device = *o.vkal_device,
        .max_sets = 10,
        .pool_size = 10,
    });

    DescriptorPoolInfo pi0 = ds->get_pool({*l0});
    DescriptorPoolInfo pi1 = ds->get_pool({*l1});

    ASSERT_EQ(ds->pools.size(), 2);

    // Pool 0
    ASSERT_EQ(ds->pools[0]->remaining_sets, 9);

    ASSERT_EQ(ds->pools[0]->pool_sizes[0].type, vk::DescriptorType::eUniformBuffer);
    ASSERT_EQ(ds->pools[0]->pool_sizes[0].descriptorCount, 8);

    ASSERT_EQ(ds->pools[0]->pool_sizes[1].type, vk::DescriptorType::eStorageImage);
    ASSERT_EQ(ds->pools[0]->pool_sizes[1].descriptorCount, 8);

    // Pool 1
    ASSERT_EQ(ds->pools[1]->remaining_sets, 9);

    ASSERT_EQ(ds->pools[1]->pool_sizes[0].type, vk::DescriptorType::eUniformBuffer);
    ASSERT_EQ(ds->pools[1]->pool_sizes[0].descriptorCount, 9);

    ASSERT_EQ(ds->pools[1]->pool_sizes[1].type, vk::DescriptorType::eStorageBuffer);
    ASSERT_EQ(ds->pools[1]->pool_sizes[1].descriptorCount, 9);

    ASSERT_EQ(ds->pools[1]->pool_sizes[2].type, vk::DescriptorType::eSampler);
    ASSERT_EQ(ds->pools[1]->pool_sizes[2].descriptorCount, 9);

    ASSERT_EQ(ds->pools[1]->pool_sizes[3].type, vk::DescriptorType::eSampledImage);
    ASSERT_EQ(ds->pools[1]->pool_sizes[3].descriptorCount, 6);

    // Checking pool infos
    ASSERT_EQ(pi0.pool_size_indices.size(), 2);
    ASSERT_EQ(pi0.pool_size_indices[0], 0);
    ASSERT_EQ(pi0.pool_size_indices[1], 1);

    ASSERT_EQ(pi1.pool_size_indices.size(), 4);
    ASSERT_EQ(pi1.pool_size_indices[0], 0);
    ASSERT_EQ(pi1.pool_size_indices[1], 1);
    ASSERT_EQ(pi1.pool_size_indices[2], 2);
    ASSERT_EQ(pi1.pool_size_indices[3], 3);

    SDL_Quit();
}

TEST(DescriptorTest, FreeSet) {
    TestObjects o = create_test_objects();

    DescriptorLayoutPtr l = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 4),
        }});

    DescriptorPtr ds = descriptor_ptr(DescriptorParams{
        .vkal_device = *o.vkal_device,
        .max_sets = 10,
        .pool_size = 10,
    });

    {
        DescriptorSetPtr dcs = descriptor_set_ptr(DescriptorSetParams{
            .vkal_device = *o.vkal_device,
            .vkal_layouts = {*l},
            .vkal_descriptor = *ds,
        });

        ASSERT_EQ(ds->pools.size(), 1);
        ASSERT_EQ(ds->pools.back()->pool_sizes.size(), 1);

        ASSERT_EQ(ds->pools.back()->pool_sizes[0].type, vk::DescriptorType::eStorageImage);
        ASSERT_EQ(ds->pools.back()->pool_sizes[0].descriptorCount, 6);
    }

    ASSERT_EQ(ds->pools.size(), 0);

    SDL_Quit();
}

TEST(DescriptorTest, FreeSetSharedPool) {
    TestObjects o = create_test_objects();

    DescriptorLayoutPtr l = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eSampler, 1),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 4),
        }});

    DescriptorPtr ds = descriptor_ptr(DescriptorParams{
        .vkal_device = *o.vkal_device,
        .max_sets = 10,
        .pool_size = 10,
    });

    DescriptorSetPtr dcs0 = descriptor_set_ptr(DescriptorSetParams{
        .vkal_device = *o.vkal_device,
        .vkal_layouts = {*l},
        .vkal_descriptor = *ds,
    });
    {
        DescriptorSetPtr dcs1 = descriptor_set_ptr(DescriptorSetParams{
            .vkal_device = *o.vkal_device,
            .vkal_layouts = {*l},
            .vkal_descriptor = *ds,
        });
    }

    ASSERT_EQ(ds->pools.size(), 1);
    ASSERT_EQ(ds->pools.back()->pool_sizes.size(), 2);

    ASSERT_EQ(ds->pools.back()->pool_sizes[0].type, vk::DescriptorType::eSampler);
    ASSERT_EQ(ds->pools.back()->pool_sizes[0].descriptorCount, 9);

    ASSERT_EQ(ds->pools.back()->pool_sizes[1].type, vk::DescriptorType::eStorageImage);
    ASSERT_EQ(ds->pools.back()->pool_sizes[1].descriptorCount, 6);

    SDL_Quit();
}

TEST(DescriptorTest, FreeSetSeparatePool) {
    TestObjects o = create_test_objects();

    DescriptorLayoutPtr l0 = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1),
        }});
    DescriptorLayoutPtr l1 = descriptor_layout_ptr(DescriptorLayoutParams{
        .vkal_device = *o.vkal_device,
        .bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 4),
        }});

    DescriptorPtr ds = descriptor_ptr(DescriptorParams{
        .vkal_device = *o.vkal_device,
        .max_sets = 10,
        .pool_size = 10,
    });

    DescriptorSetPtr dcs0 = descriptor_set_ptr(DescriptorSetParams{
        .vkal_device = *o.vkal_device,
        .vkal_layouts = {*l0},
        .vkal_descriptor = *ds,
    });
    {
        DescriptorSetPtr dcs1 = descriptor_set_ptr(DescriptorSetParams{
            .vkal_device = *o.vkal_device,
            .vkal_layouts = {*l1},
            .vkal_descriptor = *ds,
        });
    }

    ASSERT_EQ(ds->pools.size(), 1);

    SDL_Quit();
}

} // namespace vkal
