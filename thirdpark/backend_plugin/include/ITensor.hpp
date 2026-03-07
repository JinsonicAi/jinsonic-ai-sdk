#pragma once
#include <any>
#include <memory>
#include <string>
#include <vector>

class ITensor {
public:
	virtual ~ITensor() {}

	virtual int32_t GetBatch() const	  = 0;
	virtual int32_t GetWidth() const	  = 0;
	virtual int32_t GetHeight() const	  = 0;
	virtual int32_t GetChannel() const	  = 0;
	virtual int32_t GetElementNum() const = 0;

	virtual void*&	  data() const = 0;
	virtual uint64_t& phy()		   = 0;
	virtual void*&	  context()	   = 0;
	// virtual size_t size() const = 0;
	virtual std::vector<int32_t> shape() const					   = 0;
	virtual int32_t&			 dtype() const					   = 0;
	virtual std::string			 name() const					   = 0;
	virtual void				 CopyToDevice(std::any& src) const = 0;

	template <typename DType>
	inline DType* host() const {
		return static_cast<DType*>(this->data());
	};

	using TensorPtr = std::shared_ptr<ITensor>;
};
