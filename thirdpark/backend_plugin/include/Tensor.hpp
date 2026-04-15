#pragma once
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

enum {
	eDataTypeImage,
	eDataTypeBlobNhwc,	// data_ which already finished preprocess(color conversion, resize, normalize_, etc.)
	eDataTypeBlobNchw,
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

public:
	int32_t bytes() const;
	bool	isNchw() const;
	template <typename DType>
	const DType* cpu();	 // Only declaration, no implementation
	bool		 save_to_file(const std::string& file = "") const;
	bool		 load_from_file(const std::string& file = "");

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
	// static void deleteImpl(Impl* ptr);
	// std::unique_ptr<Impl, void(*)(Impl*)> impl_;
};
