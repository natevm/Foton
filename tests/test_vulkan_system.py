import pytest

import Vinyl

def test_get_init_start_stop():
    # First we'd get the vulkan system
    vulkan = Vinyl.Systems.Vulkan.Get()
    assert vulkan != None

    # Then we'd initialize that system
    assert vulkan.initialize() == True
    assert vulkan.initialize() == False
    assert vulkan.initialized == True

    # Then we'd start/stop the system during runtime.
    assert vulkan.start() == True
    assert vulkan.running == True
    assert vulkan.stop() == True
    assert vulkan.running == False

    # Can't start twice
    assert vulkan.start() == True
    assert vulkan.start() == False

    # Can't stop twice
    assert vulkan.stop() == True
    assert vulkan.stop() == False

def test_initialization_boilerplate():
    # First, get the vulkan system
    vk_system = Vinyl.Systems.Vulkan.Get()
    assert vk_system != None

    # Now we create a vulkan instance, requesting the following optional validation layer
    result = vk_system.create_vulkan_instance(["VK_LAYER_LUNARG_standard_validation"])
    assert result == True

    # Optionally add validation layers, which will report possible vulkan errors.
    def callback() :
        print("callback triggered!")
    
    result = vk_system.set_validation_callback(callback)
    assert result == True

    # Pick a device which supports the requested extensions
    # result = vk_system.pick_first_capable_device()
    # assert result == True

    # # Create a command pool, which allocates resources for vulkan command buffers
    # result = vk_system.create_command_pools()
    # assert result == True

    # Optionally create a default swapchain for rendering to a desktop window 
    # result = vk_system.CreateSwapChain()
    # assert result == True

    # result = vk_system.CreateImageViews()
    # assert result == True

    # result = vk_system.CreateGlobalRenderPass()
    # assert result == True
    
    # result = vk_system.CreateDepthResources()
    # assert result == True

    # result = vk_system.CreateFrameBuffers()
    # assert result == True

    # result = vk_system.CreateCommandBuffers()
    # assert result == True

    # result = vk_system.CreateSemaphores()
    # assert result == True
