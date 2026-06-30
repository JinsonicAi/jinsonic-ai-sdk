#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "ITensor.hpp"
#define DEBUG printf("    DEBUG:  %s:%d,	%s\r\n", __FILE__, __LINE__, __FUNCTION__);
#define CURRENT_DEVICE_ID -1

enum DataType {
	None,
	Uint4,
	Int4,
	Uint8,
	Int8,
	Uint16,
	Int16,
	Uint32,
	Int32,
	Int64,
	Fp16,
	Fp32,
};	// OutputTensorInfo: [Out] The dimentions of tensor is set from model information

enum class TensorLayout : int {
	Model = 0,
	NCHW,
	NHWC,
};

struct TensorInputSpec {
	DataType type{DataType::None};
	TensorLayout layout{TensorLayout::Model};
	bool pass_through{false};

	bool valid() const noexcept { return type != DataType::None; }
};

// Explicit numeric-tensor submission contract. layout is independent from the
// model native layout; Runtime converts when pass_through is false.
struct TensorInput {
	const void* data{nullptr};
	std::size_t bytes{0};
	DataType type{DataType::None};
	TensorLayout layout{TensorLayout::Model};
	bool pass_through{false};
	std::shared_ptr<void> owner;

	bool valid() const noexcept { return data != nullptr && bytes > 0 && type != DataType::None; }

	template <typename T>
	static TensorInput fromVector(std::vector<T> values, TensorLayout layout = TensorLayout::Model,
								bool pass_through = false) {
		static_assert(std::is_same_v<T, float> || std::is_same_v<T, uint8_t> ||
					  std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t>,
					  "TensorInput::fromVector only supports RKNN scalar input types");
		auto storage = std::make_shared<std::vector<T>>(std::move(values));
		TensorInput input;
		input.data = storage->data();
		input.bytes = storage->size() * sizeof(T);
		input.layout = layout;
		input.pass_through = pass_through;
		input.owner = std::static_pointer_cast<void>(storage);
		if constexpr (std::is_same_v<T, float>) input.type = DataType::Fp32;
		else if constexpr (std::is_same_v<T, uint8_t>) input.type = DataType::Uint8;
		else if constexpr (std::is_same_v<T, int8_t>) input.type = DataType::Int8;
		else if constexpr (std::is_same_v<T, int16_t>) input.type = DataType::Int16;
		else input.type = DataType::Int32;
		return input;
	}
};
using TensorInputs = std::vector<TensorInput>;

enum {
	eDataTypeImage,
	eDataTypeBlobNhwc,	// data_ which already finished preprocess(color conversion, resize, normalize_, etc.)
	eDataTypeBlobNchw,
};

// Native DMA input owned by the producer frame. This is populated internally
// by CopyToDevice(AXVideoFrame) on RK and consumed by RkPlugin; application
// plugins continue to pass job.input exactly as before.
struct TensorExternalMemory {
	void*					 virtual_addr{nullptr};
	std::size_t			 bytes{0};
	int					 dma_fd{-1};
	int					 offset{0};
	std::shared_ptr<void> owner;

	bool valid() const noexcept {
		return virtual_addr != nullptr && bytes > 0 && dma_fd >= 0 && owner != nullptr;
	}
};

class Tensor : public ITensor {
public:
	Tensor();
	~Tensor();
	Tensor(std::string name_, int32_t tensor_type_, bool is_nchw = true);
	Tensor(std::string name, const std::vector<int32_t>& dims,
		   int32_t dtype = ::DataType::Fp32, std::shared_ptr<void> data = nullptr,
		   bool is_nchw = true, int device_id = CURRENT_DEVICE_ID);
	// ~Tensor(){};

	int32_t GetBatch() const override;
	int32_t GetWidth() const override;
	int32_t GetHeight() const override;
	int32_t GetChannel() const override;
	int32_t GetElementNum() const override;

	void*&				 data() const override;
	uint64_t&			 phy() override;
	void*&				 context() override;
	std::vector<int32_t> shape() const override;
	int32_t&			 dtype() const override;
	std::string			 name() const override;
	void				 CopyToDevice(std::any& src) const override;

	// Internal backend hand-off for native DMA input. Upper-layer algorithms
	// should keep using job.input = std::shared_ptr<AXVideoFrame>.
	void SetExternalMemory(TensorExternalMemory memory) const;
	void ClearExternalMemory() const;
	bool GetExternalMemory(TensorExternalMemory& memory) const;
	TensorLayout submissionLayout() const;
	bool submissionPassThrough() const;
	// Prepare direct staging memory before writing tensor->host<T>(). This keeps
	// job.input empty while preserving the actual RKNN submission metadata.
	bool prepareInput(const TensorInputSpec& spec);
	void setInputSlot(int slot);
	int inputSlot() const;

public:
	int32_t bytes() const;
	bool	isNchw() const;
	template <typename DType>
	const DType* cpu();	 // 只声明，不提供实现
	bool		 save_to_file(const std::string& file = "") const;
	bool		 load_from_file(const std::string& file = "");

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
	// static void deleteImpl(Impl* ptr);
	// std::unique_ptr<Impl, void(*)(Impl*)> impl_;
};
