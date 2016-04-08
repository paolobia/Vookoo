
#ifndef VKU_INCLUDED
#define VKU_INCLUDED


#ifdef _WIN32
  #define VK_USE_PLATFORM_WIN32_KHR
  #pragma comment(lib, "vulkan/vulkan-1.lib")
  #define _CRT_SECURE_NO_WARNINGS

#endif

#include "../vulkan/vulkan.h"

#include <cstring>
#include <vector>
#include <array>
#include <fstream>

// derived from https://github.com/SaschaWillems/Vulkan

namespace vku {

class error : public std::runtime_error {
  const char *what(VkResult err) {
    switch (err) {
      case VK_SUCCESS: return "VK_SUCCESS";
      case VK_NOT_READY: return "VK_NOT_READY";
      case VK_TIMEOUT: return "VK_TIMEOUT";
      case VK_EVENT_SET: return "VK_EVENT_SET";
      case VK_EVENT_RESET: return "VK_EVENT_RESET";
      case VK_INCOMPLETE: return "VK_INCOMPLETE";
      case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
      case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
      case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
      case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
      case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
      case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
      case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
      case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
      case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
      case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
      case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
      case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
      case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
      case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
      case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
      case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
      case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
      default: return "UNKNOWN ERROR";
    }
  }
public:
  error(VkResult err) : std::runtime_error(what(err)) {
  }
};

template <class VulkanType> VulkanType create(VkDevice dev) { return nullptr; }
template <class VulkanType> void destroy(VkDevice dev, VulkanType value) {}


template <class VulkanType>
class resource {
public:
  resource() : value_(nullptr), ownsResource(false), dev_(nullptr) {
  }

  resource(VulkanType value, VkDevice dev = nullptr) : value_(value), ownsResource(false), dev_(dev) {
  }

  resource(VkDevice dev = nullptr) : value_(create<VulkanType>(dev)), ownsResource(true), dev_(dev) {
  }

  void operator=(resource &&rhs) {
    if (value_ && ownsResource) destroy<VulkanType>(dev_, value_);
    value_ = rhs.value_;
    dev_ = rhs.dev_;
    ownsResource = rhs.ownsResource;
    rhs.value_ = nullptr;
    rhs.ownsResource = false;
  }

  ~resource() {
    if (value_ && ownsResource) destroy<VulkanType>(dev_, value_);
  }

  operator VulkanType() {
    return value_;
  }

  VulkanType get() const { return value_; }
  VkDevice dev() const { return dev_; }
  resource &set(VulkanType value, bool owns) { value_ = value; ownsResource = owns; return *this; }

  void clear() { if (value_ && ownsResource) destroy<VulkanType>(dev_, value_); value_ = nullptr; ownsResource = false; }
private:
  VulkanType value_ = nullptr;
  bool ownsResource = false;
  VkDevice dev_ = nullptr;
};

template<> VkInstance create<VkInstance>(VkDevice dev) {
  bool enableValidation = false;
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "vku";
	appInfo.pEngineName = "vku";

	// Temporary workaround for drivers not supporting SDK 1.0.3 upon launch
	// todo : Use VK_API_VERSION 
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

	std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

  #ifdef _WIN32
    enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
  #else
    // todo : linux/android
    enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
  #endif

	// todo : check if all extensions are present

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (enabledExtensions.size() > 0)
	{
		if (enableValidation)
		{
			enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	}
	if (enableValidation)
	{
		//instanceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount; // todo : change validation layer names!
		//instanceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	}

  VkInstance inst = nullptr;
	vkCreateInstance(&instanceCreateInfo, nullptr, &inst);
  return inst;
}

template<> void destroy<VkInstance>(VkDevice dev, VkInstance inst) {
  vkDestroyInstance(inst, nullptr);
}

class device {
public:
  device(VkDevice dev, VkPhysicalDevice physicalDevice_) : dev(dev), physicalDevice_(physicalDevice_) {
  }

  uint32_t getMemoryType(uint32_t typeBits, VkFlags properties) {
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  	vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &deviceMemoryProperties);

	  for (uint32_t i = 0; i < 32; i++) {
		  if (typeBits & (1<<i)) {
			  if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				  return i;
			  }
		  }
	  }
	  return ~(uint32_t)0;
  }

	VkFormat getSupportedDepthFormat()
	{
		// Since all depth formats may be optional, we need to find a suitable depth format to use
		// Start with the highest precision packed format
		static const VkFormat depthFormats[] = { 
			VK_FORMAT_D32_SFLOAT_S8_UINT, 
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT, 
			VK_FORMAT_D16_UNORM_S8_UINT, 
			VK_FORMAT_D16_UNORM
		};

		for (size_t i = 0; i != sizeof(depthFormats)/sizeof(depthFormats[0]); ++i) {
      VkFormat format = depthFormats[i];
			VkFormatProperties formatProps;
			vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &formatProps);
			// Format must support depth stencil attachment for optimal tiling
			if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				return format;
			}
		}

		return VK_FORMAT_UNDEFINED;
	}

  operator VkDevice() const { return dev; }
  VkPhysicalDevice physicalDevice() const { return physicalDevice_; }

  void waitIdle() const {
    vkDeviceWaitIdle(dev);
  }
public:
  VkDevice dev;
  VkPhysicalDevice physicalDevice_;
};

class instance : public resource<VkInstance> {
public:
  instance() : resource((VkInstance)nullptr) {
  }

  /// instance that does not own its pointer
  instance(VkInstance value) : resource(value, nullptr) {
  }

  /// instance that does owns (and creates) its pointer
  instance(const char *name) : resource((VkDevice)nullptr) {
	  // Physical device
	  uint32_t gpuCount = 0;
	  // Get number of available physical devices
	  VkResult err = vkEnumeratePhysicalDevices(get(), &gpuCount, nullptr);
    if (err) throw error(err);

    if (gpuCount == 0) {
      throw(std::runtime_error("no Vulkan devices found"));
    }

	  // Enumerate devices
	  std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	  err = vkEnumeratePhysicalDevices(get(), &gpuCount, physicalDevices.data());
    if (err) throw error(err);

	  // Note : 
	  // This example will always use the first physical device reported, 
	  // change the vector index if you have multiple Vulkan devices installed 
	  // and want to use another one
	  physicalDevice_ = physicalDevices[0];

	  // Find a queue that supports graphics operations
	  uint32_t graphicsQueueIndex = 0;
	  uint32_t queueCount;
	  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice(), &queueCount, NULL);
	  //assert(queueCount >= 1);

	  std::vector<VkQueueFamilyProperties> queueProps;
	  queueProps.resize(queueCount);
	  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice(), &queueCount, queueProps.data());

	  for (graphicsQueueIndex = 0; graphicsQueueIndex < queueCount; graphicsQueueIndex++)
	  {
		  if (queueProps[graphicsQueueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			  break;
	  }
	  //assert(graphicsQueueIndex < queueCount);

	  // Vulkan device
	  std::array<float, 1> queuePriorities = { 0.0f };
	  VkDeviceQueueCreateInfo queueCreateInfo = {};
	  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	  queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
	  queueCreateInfo.queueCount = 1;
	  queueCreateInfo.pQueuePriorities = queuePriorities.data();

	  std::vector<const char*> enabledExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	  VkDeviceCreateInfo deviceCreateInfo = {};
	  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	  deviceCreateInfo.pNext = NULL;
	  deviceCreateInfo.queueCreateInfoCount = 1;
	  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	  deviceCreateInfo.pEnabledFeatures = NULL;

	  if (enabledExtensions.size() > 0)
	  {
		  deviceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
		  deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	  }
	  if (enableValidation)
	  {
		  //deviceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount; // todo : validation layer names
		  //deviceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	  }

	  err = vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &dev_);
    if (err) throw error(err);

	  // Get the graphics queue
	  vkGetDeviceQueue(dev_, graphicsQueueIndex, 0, &queue_);
  }

  VkPhysicalDevice physicalDevice() const { return physicalDevice_; }

  vku::device device() const { return vku::device(dev_, physicalDevice_); }
  VkQueue queue() const { return queue_; }

public:
  bool enableValidation = false;
  VkPhysicalDevice physicalDevice_;
  VkDevice dev_;
  VkQueue queue_;
};

template<> void destroy<VkSwapchainKHR>(VkDevice dev, VkSwapchainKHR chain) {
  vkDestroySwapchainKHR(dev, chain, nullptr);
}

class swapChain : public resource<VkSwapchainKHR> {
public:
  /// semaphore that does not own its pointer
  swapChain(VkSwapchainKHR value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// semaphore that does owns (and creates) its pointer
  swapChain(const vku::device dev, uint32_t width, uint32_t height, VkSurfaceKHR surface, VkCommandBuffer buf) : resource(dev) {
	  VkResult err;
	  VkSwapchainKHR oldSwapchain = *this;

	  // Get physical device surface properties and formats
	  VkSurfaceCapabilitiesKHR surfCaps;
	  err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev.physicalDevice(), surface, &surfCaps);
    if (err) throw error(err);

	  uint32_t presentModeCount;
	  err = vkGetPhysicalDeviceSurfacePresentModesKHR(dev.physicalDevice(), surface, &presentModeCount, NULL);
    if (err) throw error(err);

	  // todo : replace with vector?
	  VkPresentModeKHR *presentModes = (VkPresentModeKHR *)malloc(presentModeCount * sizeof(VkPresentModeKHR));

	  err = vkGetPhysicalDeviceSurfacePresentModesKHR(dev.physicalDevice(), surface, &presentModeCount, presentModes);
    if (err) throw error(err);

	  VkExtent2D swapchainExtent = {};
	  // width and height are either both -1, or both not -1.
	  if (surfCaps.currentExtent.width == -1)
	  {
		  // If the surface size is undefined, the size is set to
		  // the size of the images requested.
		  width_ = swapchainExtent.width = width;
		  height_ = swapchainExtent.height = height;
	  }
	  else
	  {
		  // If the surface size is defined, the swap chain size must match
		  swapchainExtent = surfCaps.currentExtent;
		  width_ = surfCaps.currentExtent.width;
		  height_ = surfCaps.currentExtent.height;
	  }

	  // Try to use mailbox mode
	  // Low latency and non-tearing
	  VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	  for (size_t i = 0; i < presentModeCount; i++) 
	  {
		  if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) 
		  {
			  swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			  break;
		  }
		  if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)) 
		  {
			  swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		  }
	  }

	  // Determine the number of images
	  uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
	  if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
	  {
		  desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
	  }

	  VkSurfaceTransformFlagsKHR preTransform;
	  if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	  {
		  preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	  }
	  else {
		  preTransform = surfCaps.currentTransform;
	  }

	  VkSwapchainCreateInfoKHR swapchainCI = {};
	  swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	  swapchainCI.pNext = NULL;
	  swapchainCI.surface = surface;
	  swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
	  swapchainCI.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	  swapchainCI.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	  swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
	  swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	  swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
	  swapchainCI.imageArrayLayers = 1;
	  swapchainCI.queueFamilyIndexCount = VK_SHARING_MODE_EXCLUSIVE;
	  swapchainCI.queueFamilyIndexCount = 0;
	  swapchainCI.pQueueFamilyIndices = NULL;
	  swapchainCI.presentMode = swapchainPresentMode;
	  swapchainCI.oldSwapchain = oldSwapchain;
	  swapchainCI.clipped = true;
	  swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainKHR res;
	  err = vkCreateSwapchainKHR(dev, &swapchainCI, nullptr, &res);
	  if (err) throw error(err);
    set(res, true);

    build_images(buf);
  }

  void build_images(VkCommandBuffer buf);

  swapChain &operator=(swapChain &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    width_ = rhs.width_;
    height_ = rhs.height_;
    swapchainImages = std::move(rhs.swapchainImages);
    swapchainViews = std::move(rhs.swapchainViews);
    return *this;
  }

	void present(VkQueue queue, uint32_t currentBuffer)
	{
		VkPresentInfoKHR presentInfo = {};
    VkSwapchainKHR sc = *this;
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = NULL;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &sc;
		presentInfo.pImageIndices = &currentBuffer;
    VkResult err = vkQueuePresentKHR(queue, &presentInfo);
    if (err) throw error(err);
	}

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

  size_t imageCount() { return swapchainImages.size(); }
  VkImage image(size_t i) { return swapchainImages[i]; }
  VkImageView view(size_t i) { return swapchainViews[i]; }
private:
  uint32_t width_;
  uint32_t height_;
	//VkFormat colorFormat;
	//VkColorSpaceKHR colorSpace;

  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainViews;

	//VkImage* swapchainImages;

	//VkSwapchainKHR swapChain = VK_NULL_HANDLE;
  //vku::swapChain swapChain;

	//uint32_t imageCount;
	//SwapChainBuffer* buffers;

	// Index of the deteced graphics and presenting device queue
	//uint32_t queueNodeIndex = UINT32_MAX;
};

class buffer {
public:
  buffer(VkDevice dev = nullptr, VkBuffer buf = nullptr) : buf_(buf), dev(dev) {
  }

  buffer(VkDevice dev, VkBufferCreateInfo *bufInfo) : dev(dev), size_(bufInfo->size) {
		vkCreateBuffer(dev, bufInfo, nullptr, &buf_);
    ownsBuffer = true;
  }

  buffer(device dev, void *init, VkDeviceSize size, VkBufferUsageFlags usage) : dev(dev), size_(size) {
    VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = size;
		bufInfo.usage = usage;
		VkResult err = vkCreateBuffer(dev, &bufInfo, nullptr, &buf_);
    if (err) throw error(err);

    ownsBuffer = true;

		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		vkGetBufferMemoryRequirements(dev, buf_, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = dev.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

 		err = vkAllocateMemory(dev, &memAlloc, nullptr, &mem);
    if (err) throw error(err);

		if (init) {
		  void *dest = map();
      std::memcpy(dest, init, size);
      unmap();
    }
    bind();
  }

  buffer(VkBufferCreateInfo bufInfo, VkDevice dev = nullptr) : dev(dev) {
		vkCreateBuffer(dev, &bufInfo, nullptr, &buf_);
  }

  // RAII move operator
  buffer &operator=(buffer &&rhs) {
    dev = rhs.dev;
    buf_ = rhs.buf_;
    mem = rhs.mem;
    size_ = rhs.size_;
    rhs.dev = nullptr;
    rhs.mem = nullptr;
    rhs.buf_ = nullptr;

    rhs.ownsBuffer = false;
    return *this;
  }

  ~buffer() {
    if (buf_ && ownsBuffer) {
      vkDestroyBuffer(dev, buf_, nullptr);
      buf_ = nullptr;
    }
  }

  void *map() {
    void *dest = nullptr;
    VkResult err = vkMapMemory(dev, mem, 0, size(), 0, &dest);
    if (err) throw error(err);
    return dest;
  }

  void unmap() {
		vkUnmapMemory(dev, mem);
  }

  void bind() {
		VkResult err = vkBindBufferMemory(dev, buf_, mem, 0);
    if (err) throw error(err);
  }

  size_t size() const {
    return size_;
  }

  //operator VkBuffer() const { return buf_; }

  VkBuffer buf() const { return buf_; }

  VkDescriptorBufferInfo desc() const {
    VkDescriptorBufferInfo d = {};
    d.buffer = buf_;
    d.range = size_;
    return d;
  }

private:
  VkBuffer buf_ = nullptr;
  VkDevice dev = nullptr;
  VkDeviceMemory mem = nullptr;
  size_t size_;
  bool ownsBuffer = false;
};

class vertexInputState {
public:
  vertexInputState() {
  }

  vertexInputState &operator=(vertexInputState && rhs) {
    vi = rhs.vi;
    bindingDescriptions = std::move(rhs.bindingDescriptions);
    attributeDescriptions = std::move(rhs.attributeDescriptions);
  }

  vertexInputState &attrib(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription desc = {};
    desc.location = location;
    desc.binding = binding;
    desc.format = format;
    desc.offset = offset;
    attributeDescriptions.push_back(desc);
    return *this;
  }

  vertexInputState &binding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX) {
    VkVertexInputBindingDescription desc = {};
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = inputRate;
    bindingDescriptions.push_back(desc);
    return *this;
  }

  VkPipelineVertexInputStateCreateInfo *get() {
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi.pNext = nullptr;
		vi.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
		vi.pVertexBindingDescriptions = bindingDescriptions.data();
		vi.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
		vi.pVertexAttributeDescriptions = attributeDescriptions.data();
    return &vi;
  }

private:
	VkPipelineVertexInputStateCreateInfo vi;
	std::vector<VkVertexInputBindingDescription> bindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
};

class descriptorPool {
public:
  descriptorPool() {
  }

  descriptorPool(VkDevice dev) : dev_(dev) {
		// We need to tell the API the number of max. requested descriptors per type
		VkDescriptorPoolSize typeCounts[1];
		// This example only uses one descriptor type (uniform buffer) and only
		// requests one descriptor of this type
		typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		typeCounts[0].descriptorCount = 2;
		// For additional types you need to add new entries in the type count list
		// E.g. for two combined image samplers :
		// typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		// typeCounts[1].descriptorCount = 2;

		// Create the global descriptor pool
		// All descriptors used in this example are allocated from this pool
		VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.pNext = NULL;
		descriptorPoolInfo.poolSizeCount = 1;
		descriptorPoolInfo.pPoolSizes = typeCounts;
		// Set the max. number of sets that can be requested
		// Requesting descriptors beyond maxSets will result in an error
		descriptorPoolInfo.maxSets = 2;

		VkResult err = vkCreateDescriptorPool(dev_, &descriptorPoolInfo, nullptr, &pool_);
    if (err) throw error(err);

    ownsResource_ = true;
  }

  // allocate a descriptor set for a buffer
  VkWriteDescriptorSet *allocateDescriptorSet(const buffer &buffer, const VkDescriptorSetLayout *layout, VkDescriptorSet *descriptorSets) {
		// Update descriptor sets determining the shader binding points
		// For every binding point used in a shader there needs to be one
		// descriptor set matching that binding point

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool_;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layout;

		VkResult err = vkAllocateDescriptorSets(dev_, &allocInfo, descriptorSets);
    if (err) throw error(err);

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSets[0];
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &buffer.desc();
		// Binds this uniform buffer to binding point 0
		writeDescriptorSet.dstBinding = 0;

    return &writeDescriptorSet;
  }

  ~descriptorPool() {
    if (pool_ && ownsResource_) {
      vkDestroyDescriptorPool(dev_, pool_, nullptr);
      ownsResource_ = false;
    }
  }

  descriptorPool &operator=(descriptorPool &&rhs) {
    ownsResource_ = true;
    pool_ = rhs.pool_;
    rhs.ownsResource_ = false;
    dev_ = rhs.dev_;
    return *this;
  }

  operator VkDescriptorPool() { return pool_; }
private:
  VkDevice dev_ = nullptr;
  VkDescriptorPool pool_ = nullptr;
  bool ownsResource_ = false;
  VkWriteDescriptorSet writeDescriptorSet = {};
};


class pipeline {
public:
  pipeline() {
  }

  pipeline(VkDevice device, VkRenderPass renderPass, VkPipelineVertexInputStateCreateInfo *vertexInputState, VkPipelineCache pipelineCache) : dev_(device) {
		// Setup layout of descriptors used in this example
		// Basically connects the different shader stages to descriptors
		// for binding uniform buffers, image samplers, etc.
		// So every shader binding should map to one descriptor set layout
		// binding

		// Binding 0 : Uniform buffer (Vertex shader)
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBinding.pImmutableSamplers = NULL;

		VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
		descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayout.pNext = NULL;
		descriptorLayout.bindingCount = 1;
		descriptorLayout.pBindings = &layoutBinding;

		VkResult err = vkCreateDescriptorSetLayout(device, &descriptorLayout, NULL, &descriptorSetLayout);
		if (err) throw error(err);

		// Create the pipeline layout that is used to generate the rendering pipelines that
		// are based on this descriptor set layout
		// In a more complex scenario you would have different pipeline layouts for different
		// descriptor set layouts that could be reused
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
		pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.pNext = NULL;
		pPipelineLayoutCreateInfo.setLayoutCount = 1;
		pPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

		err = vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		if (err) throw error(err);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};

		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		// The layout used for this pipeline
		pipelineCreateInfo.layout = pipelineLayout;
		// Renderpass this pipeline is attached to
		pipelineCreateInfo.renderPass = renderPass;

		// Vertex input state
		// Describes the topoloy used with this pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
		inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		// This pipeline renders vertex data as triangle lists
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// Rasterization state
		VkPipelineRasterizationStateCreateInfo rasterizationState = {};
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		// Solid polygon mode
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		// No culling
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationState.depthClampEnable = VK_FALSE;
		rasterizationState.rasterizerDiscardEnable = VK_FALSE;
		rasterizationState.depthBiasEnable = VK_FALSE;

		// Color blend state
		// Describes blend modes and color masks
		VkPipelineColorBlendStateCreateInfo colorBlendState = {};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		// One blend attachment state
		// Blending is not used in this example
		VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
		blendAttachmentState[0].colorWriteMask = 0xf;
		blendAttachmentState[0].blendEnable = VK_FALSE;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = blendAttachmentState;

		// Viewport state
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		// One viewport
		viewportState.viewportCount = 1;
		// One scissor rectangle
		viewportState.scissorCount = 1;

		// Enable dynamic states
		// Describes the dynamic states to be used with this pipeline
		// Dynamic states can be set even after the pipeline has been created
		// So there is no need to create new pipelines just for changing
		// a viewport's dimensions or a scissor box
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		// The dynamic state properties themselves are stored in the command buffer
		std::vector<VkDynamicState> dynamicStateEnables;
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

		// Depth and stencil state
		// Describes depth and stenctil test and compare ops
		VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
		// Basic depth compare setup with depth writes and depth test enabled
		// No stencil used 
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
		depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
		depthStencilState.stencilTestEnable = VK_FALSE;
		depthStencilState.front = depthStencilState.back;

		// Multi sampling state
		VkPipelineMultisampleStateCreateInfo multisampleState = {};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.pSampleMask = NULL;
		// No multi sampling used in this example
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Load shaders
		VkPipelineShaderStageCreateInfo shaderStages[2] = { {},{} };

#ifdef USE_GLSL
		shaderStages[0] = loadShaderGLSL("data/shaders/triangle.vert", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShaderGLSL("data/shaders/triangle.frag", VK_SHADER_STAGE_FRAGMENT_BIT);
#else
		shaderStages[0] = loadShader("data/shaders/triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader("data/shaders/triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
#endif

		// Assign states
		// Two shader stages
		pipelineCreateInfo.stageCount = 2;
		// Assign pipeline state create information
		pipelineCreateInfo.pVertexInputState = vertexInputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pStages = shaderStages;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.pDynamicState = &dynamicState;

		// Create rendering pipeline
		err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipe_);
		if (err) throw error(err);

    ownsData = true;
  }

  pipeline &operator=(pipeline &&rhs) {
    pipe_ = rhs.pipe_;
	  pipelineLayout = rhs.pipelineLayout;
	  descriptorSet = rhs.descriptorSet;
	  descriptorSetLayout = rhs.descriptorSetLayout;
    dev_ = rhs.dev_;
	  shaderModules = std::move(shaderModules);
    ownsData = true;
    rhs.ownsData = false;
    return *this;
  }

  ~pipeline() {
    if (ownsData) {
		  vkDestroyPipeline(dev_, pipe_, nullptr);
		  vkDestroyPipelineLayout(dev_, pipelineLayout, nullptr);
		  vkDestroyDescriptorSetLayout(dev_, descriptorSetLayout, nullptr);
    }
  }

  void allocateDescriptorSets(descriptorPool &descPool) {
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

    descriptorSet = nullptr;
		VkResult err = vkAllocateDescriptorSets(dev_, &allocInfo, &descriptorSet);
		if (err) throw error(err);
  }

  void updateDescriptorSets(buffer &uniformVS) {
		VkWriteDescriptorSet writeDescriptorSet = {};

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &uniformVS.desc();
		// Binds this uniform buffer to binding point 0
		writeDescriptorSet.dstBinding = 0;

		vkUpdateDescriptorSets(dev_, 1, &writeDescriptorSet, 0, NULL);
  }

  VkPipeline pipe() { return pipe_; }
  VkPipelineLayout layout() const { return pipelineLayout; }
  VkDescriptorSet *descriptorSets() { return &descriptorSet; }
  VkDescriptorSetLayout *descriptorLayouts() { return &descriptorSetLayout; }

private:
  VkPipelineShaderStageCreateInfo loadShader(const char * fileName, VkShaderStageFlagBits stage)
  {
    std::ifstream input(fileName, std::ios::binary);
    auto &b = std::istreambuf_iterator<char>(input);
    auto &e = std::istreambuf_iterator<char>();
    std::vector<char> buf(b, e);

		VkShaderModuleCreateInfo moduleCreateInfo = {};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = buf.size();
		moduleCreateInfo.pCode = (uint32_t*)buf.data();
		moduleCreateInfo.flags = 0;
		VkShaderModule shaderModule;
		VkResult err = vkCreateShaderModule(dev_, &moduleCreateInfo, NULL, &shaderModule);
    if (err) throw error(err);

	  VkPipelineShaderStageCreateInfo shaderStage = {};
	  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	  shaderStage.stage = stage;
	  shaderStage.module = shaderModule;
	  shaderStage.pName = "main"; // todo : make param
	  shaderModules.push_back(shaderStage.module);
	  return shaderStage;
  }

  VkPipelineShaderStageCreateInfo loadShaderGLSL(const char * fileName, VkShaderStageFlagBits stage)
  {
    std::ifstream input(fileName, std::ios::binary);
    auto &b = std::istreambuf_iterator<char>(input);
    auto &e = std::istreambuf_iterator<char>();

    std::vector<char> buf;
    buf.resize(12);
		((uint32_t *)buf.data())[0] = 0x07230203; 
		((uint32_t *)buf.data())[1] = 0;
		((uint32_t *)buf.data())[2] = stage;
    while (b != e) buf.push_back(*b++);
    buf.push_back(0);

		VkShaderModuleCreateInfo moduleCreateInfo = {};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = buf.size();
		moduleCreateInfo.pCode = (uint32_t*)buf.data();
		moduleCreateInfo.flags = 0;

		VkShaderModule shaderModule;
		VkResult err = vkCreateShaderModule(dev_, &moduleCreateInfo, NULL, &shaderModule);
    if (err) throw error(err);

	  VkPipelineShaderStageCreateInfo shaderStage = {};
	  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	  shaderStage.stage = stage;
	  shaderStage.module = shaderModule;
	  shaderStage.pName = "main";
	  shaderModules.push_back(shaderStage.module);
	  return shaderStage;
  }

  VkPipeline pipe_ = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;
	VkDescriptorSet descriptorSet = nullptr;
	VkDescriptorSetLayout descriptorSetLayout = nullptr;
  VkDevice dev_ = nullptr;
	std::vector<VkShaderModule> shaderModules;
  bool ownsData = false;
};

class cmdBuffer : public resource<VkCommandBuffer> {
public:
  /// command buffer that does not own its pointer
  cmdBuffer(VkCommandBuffer value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// command buffer that does owns (and creates) its pointer
  cmdBuffer(VkDevice dev) : resource(dev) {
  }

  void begin(VkRenderPass renderPass, VkFramebuffer framebuffer, int width, int height) {
    beginCommandBuffer();
    beginRenderPass(renderPass, framebuffer, 0, 0, width, height);
    setViewport(0, 0, (float)width, (float)height);
    setScissor(0, 0, width, height);
  }

  void end(VkImage image) {
    endRenderPass();
    addPresentationBarrier(image);
    endCommandBuffer();
  }

  void beginCommandBuffer() {
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.pNext = NULL;
    vkBeginCommandBuffer(*this, &cmdBufInfo);
  }

  void beginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer, int x = 0, int y = 0, int width = 256, int height = 256) {
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.025f, 0.025f, 0.025f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.framebuffer = framebuffer;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = x;
		renderPassBeginInfo.renderArea.offset.y = y;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(*this, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void setViewport(float x=0, float y=0, float width=256, float height=256, float minDepth=0, float maxDepth=1) {
		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.x = x;
		viewport.y = y;
		viewport.width = width;
		viewport.height = height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		vkCmdSetViewport(*this, 0, 1, &viewport);
  }

  void setScissor(int x=0, int y=0, int width=256, int height=256) {
		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.offset.x = x;
		scissor.offset.y = y;
		scissor.extent.width = width;
		scissor.extent.height = height;
		vkCmdSetScissor(*this, 0, 1, &scissor);
  }

  void bindPipeline(pipeline &pipe) {
		// Bind descriptor sets describing shader binding points
		vkCmdBindDescriptorSets(*this, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout(), 0, 1, pipe.descriptorSets(), 0, NULL);

		// Bind the rendering pipeline (including the shaders)
		vkCmdBindPipeline(*this, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipe());
  }

  void bindVertexBuffer(buffer &buf, int bindId) {
		VkDeviceSize offsets[] = { 0 };
    VkBuffer bufs[] = { buf.buf() };
		vkCmdBindVertexBuffers(*this, bindId, 1, bufs, offsets);
  }

  void bindIndexBuffer(buffer &buf) {
		// Bind triangle indices
		vkCmdBindIndexBuffer(*this, buf.buf(), 0, VK_INDEX_TYPE_UINT32);
  }

  void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
		// Draw indexed triangle
		vkCmdDrawIndexed(*this, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
  }

  void endRenderPass() {
    vkCmdEndRenderPass(*this);
  }

  void addPresentationBarrier(VkImage image) {
		// Add a present memory barrier to the end of the command buffer
		// This will transform the frame buffer color attachment to a
		// new layout for presenting it to the windowing system integration 
		VkImageMemoryBarrier prePresentBarrier = {};
		prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		prePresentBarrier.pNext = NULL;
		prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		prePresentBarrier.dstAccessMask = 0;
		prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		prePresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };			
		prePresentBarrier.image = image;

		VkImageMemoryBarrier *pMemoryBarrier = &prePresentBarrier;
		vkCmdPipelineBarrier(
			*this, 
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
			0,
			0, nullptr,
			0, nullptr,
			1, &prePresentBarrier);
  }

  void endCommandBuffer() {
    vkEndCommandBuffer(*this);
  }

  void pipelineBarrier(VkImage image) {
		// Add a post present image memory barrier
		// This will transform the frame buffer color attachment back
		// to it's initial layout after it has been presented to the
		// windowing system
		// See buildCommandBuffers for the pre present barrier that 
		// does the opposite transformation 
		VkImageMemoryBarrier postPresentBarrier = {};
		postPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		postPresentBarrier.pNext = NULL;
		postPresentBarrier.srcAccessMask = 0;
		postPresentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		postPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		postPresentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		postPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postPresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		postPresentBarrier.image = image;

		// Use dedicated command buffer from example base class for submitting the post present barrier
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		// Put post present barrier into command buffer
		vkCmdPipelineBarrier(
			*this,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &postPresentBarrier
    );
  }

	// Create an image memory barrier for changing the layout of
	// an image and put it into an active command buffer
	// See chapter 11.4 "Image Layout" for details
	//todo : rename
	void setImageLayout(VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout)
	{
		// Create an image barrier object
	  VkImageMemoryBarrier imageMemoryBarrier = {};
	  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange.aspectMask = aspectMask;
		imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.layerCount = 1;

		// Source layouts (old)

		// Undefined layout
		// Only allowed as initial layout!
		// Make sure any writes to the image have been finished
		if (oldImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		// Old layout is color attachment
		// Make sure any writes to the color buffer have been finished
		if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) 
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		// Old layout is transfer source
		// Make sure any reads from the image have been finished
		if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		// Old layout is shader read (sampler, input attachment)
		// Make sure any shader reads from the image have been finished
		if (oldImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}

		// Target layouts (new)

		// New layout is transfer destination (copy, blit)
		// Make sure any copyies to the image have been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		// New layout is transfer source (copy, blit)
		// Make sure any reads from and writes to the image have been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		// New layout is color attachment
		// Make sure any writes to the color buffer hav been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		// New layout is depth attachment
		// Make sure any writes to depth/stencil buffer have been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
		{
			imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		// New layout is shader read (sampler, input attachment)
		// Make sure any writes to the image have been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}


		// Put barrier on top
		VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		// Put barrier inside setup command buffer
		vkCmdPipelineBarrier(
			get(), 
			srcStageFlags, 
			destStageFlags, 
			0, 
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
	}
private:
};

inline void swapChain::build_images(VkCommandBuffer buf) {
  VkDevice d = dev();
  vku::cmdBuffer cb(buf, d);

  uint32_t imageCount = 0;
	VkResult err = vkGetSwapchainImagesKHR(dev(), get(), &imageCount, NULL);
	if (err) throw error(err);

  swapchainImages.resize(imageCount);
  swapchainViews.resize(imageCount);
	err = vkGetSwapchainImagesKHR(dev(), *this, &imageCount, swapchainImages.data());
	if (err) throw error(err);

	for (uint32_t i = 0; i < imageCount; i++) {
		VkImageViewCreateInfo colorAttachmentView = {};
		colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = VK_FORMAT_B8G8R8A8_UNORM;
		colorAttachmentView.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorAttachmentView.flags = 0;


    cb.setImageLayout(
			image(i), 
			VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		colorAttachmentView.image = image(i);

		err = vkCreateImageView(dev(), &colorAttachmentView, nullptr, &swapchainViews[i]);
  	if (err) throw error(err);
  }
}


template<> VkSemaphore create<VkSemaphore>(VkDevice dev) {
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphore res = nullptr;
	VkResult err = vkCreateSemaphore(dev, &info, nullptr, &res);
	if (err) throw error(err);
  return res;
}

template<> void destroy<VkSemaphore>(VkDevice dev, VkSemaphore sema) {
  vkDestroySemaphore(dev, sema, nullptr);
}

class semaphore : public resource<VkSemaphore> {
public:
  /// semaphore that does not own its pointer
  semaphore(VkSemaphore value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// semaphore that does owns (and creates) its pointer
  semaphore(VkDevice dev) : resource(dev) {
  }
};

template<> VkQueue create<VkQueue>(VkDevice dev) { return nullptr; }
template<> void destroy<VkQueue>(VkDevice dev, VkQueue queue_) { }

class queue : public resource<VkQueue> {
public:
  /// queue that does not own its pointer
  queue(VkQueue value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// queue that does owns (and creates) its pointer
  queue(VkDevice dev) : resource(dev) {
  }

  void submit(VkSemaphore sema, VkCommandBuffer buffer) const {
		// The submit infor strcuture contains a list of
		// command buffers and semaphores to be submitted to a queue
		// If you want to submit multiple command buffers, pass an array
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = sema ? 1 : 0;
		submitInfo.pWaitSemaphores = &sema;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &buffer;

		// Submit to the graphics queue
		VkResult err = vkQueueSubmit(get(), 1, &submitInfo, VK_NULL_HANDLE);
    if (err) throw error(err);
  }

  void waitIdle() const {  
		VkResult err = vkQueueWaitIdle(get());
    if (err) throw error(err);
  }

};

} // vku

#endif
