#pragma once
#include "layer_t.h"
#include "range_t.h"

class pool_layer_t: public layer_t
{
public:
	const uint16_t stride;
	const uint16_t filter_size;
	float pad;
	pool_layer_t( uint16_t stride, uint16_t filter_size, float pad, tdsize in_size )
		:
		layer_t(in_size, tdsize(ROUND_UP_IDIV(in_size.x, stride),
					ROUND_UP_IDIV(in_size.y, stride),
					in_size.z)),
		stride(stride),
		filter_size(filter_size),
		pad(pad)
	{
		throw_assert(filter_size >= stride, "Pool filter size (" << filter_size << ") must be >= stride (" << stride << ").");
	}

	std::string kind_str() const {
		return "pool";
	}
	std::string param_str() const {
		std::stringstream ss;
		ss << "stride=" << stride << ", filter_size=" << filter_size  << ", pad=" << pad;
		return ss.str();
	}

	bool operator==(const pool_layer_t & o) const {
		if (o.stride != stride) return false;
		if (o.filter_size != filter_size) return false;
		if (o.in != in) return false;
		if (o.grads_in != grads_in) return false;
		if (o.out != out) return false;
		return true;
	}

	bool operator!=(const pool_layer_t & o) const {
		return !(*this == o);
	}

	range_t map_to_output( int x, int y )
	{
		return map_to_output_impl(x, y, filter_size, stride, out.size.z, out.size);
	}

	void activate(const tensor_t<float>& in ) {
		copy_input(in);
		for ( int x = 0; x < out.size.x; x++ )
		{
			for ( int y = 0; y < out.size.y; y++ )
			{
				for ( int z = 0; z < out.size.z; z++ )
				{
					point_t mapped(x*stride, y*stride, 0);
					float mval = -FLT_MAX;
					for ( int i = 0; i < filter_size; i++ )
						for ( int j = 0; j < filter_size; j++ )
						{
							float v;
							if (mapped.x + i >= in.size.x ||
							    mapped.y + j >= in.size.y) {
								v = pad;
							} else {
								v = in( mapped.x + i, mapped.y + j, z );
							}

							if ( v > mval )
								mval = v;
						}
					out( x, y, z ) = mval;
				}
			}
		}
	}

	void fix_weights()
	{

	}

	void calc_grads( tensor_t<float>& grad_next_layer )
	{
		for ( int x = 0; x < in.size.x; x++ )
		{
			for ( int y = 0; y < in.size.y; y++ )
			{
				range_t rn = map_to_output( x, y );
				for ( int z = 0; z < in.size.z; z++ )
				{
					float sum_error = 0;
					for ( int i = rn.min_x; i <= rn.max_x; i++ )
					{
						for ( int j = rn.min_y; j <= rn.max_y; j++ )
						{
							int is_max = in( x, y, z ) == out( i, j, z ) ? 1 : 0;
							sum_error += is_max * grad_next_layer( i, j, z );
						}
					}
					grads_in( x, y, z ) = sum_error;
				}
			}
		}
	}
};

class pool_layer_opt_t: public pool_layer_t
{
public:
	pool_layer_opt_t( uint16_t stride, uint16_t filter_size, float pad, tdsize in_size ) : pool_layer_t(stride, filter_size, pad, in_size) {}
};

#ifdef INCLUDE_TESTS
namespace CNNTest{

	TEST_F(CNNTest, pool_simple) {
		
		tdsize size(10,10,10);
		pool_layer_t t1(2, 4, 0, size);
		pool_layer_t t2(2, 4, 0, size);
		tensor_t<float> in(size);
		randomize(in);
		t1.activate(in);
		EXPECT_EQ(t1,t1);
		EXPECT_NE(t1,t2);

		pool_layer_t t3(4, 5, 0, tdsize(17,17,1));
		EXPECT_EQ(t3.out.size.x, 5);

		auto r1 =  t3.map_to_output(0,0);
		EXPECT_EQ(r1.min_x, 0);
		EXPECT_EQ(r1.max_x, 0);
		EXPECT_EQ(r1.min_y, 0);
		EXPECT_EQ(r1.max_y, 0);
		EXPECT_EQ(r1.max_z, t3.out.size.z - 1);
		
	}

	pool_layer_t pool_sized(int x, int y, int z, int ksize, int stride) {
		tdsize size(x,y,z);
		
		tensor_t<float> in(size.x, size.y, size.z);
		tensor_t<float> next_grads(ROUND_UP_IDIV(in.size.x, stride),
					   ROUND_UP_IDIV(in.size.y, stride),
					   in.size.z);
		
		randomize(in);
		randomize(next_grads);

		// Run the optimized version
		srand(42);
		pool_layer_opt_t o_layer( stride, ksize, 0, in.size);
		o_layer.activate(in);
		o_layer.calc_grads(next_grads);
		o_layer.fix_weights();
		
		// Run the reference version
		srand(42);
		pool_layer_t layer(stride, ksize, 0, in.size);
		layer.activate(in);
		layer.calc_grads(next_grads);
		layer.fix_weights();

		// Check for equality.
		EXPECT_EQ(layer, o_layer);
		return layer;
	}

	TEST_F(CNNTest, pool_sizes) {
		// Check a range of sizes, especially non-round numbers.
		pool_sized(4, 4, 4, 2, 1);

		pool_sized(1, 1, 1, 1, 1);
		//EXPECT_THROW(pool_sized(1,1,1,7,1), AssertionFailureException); // kernel too big
		//pool_sized(1,1,1,1,7);
		//EXPECT_THROW(pool_sized(2,1,1,1,7), AssertionFailureException); // stride does not divide size
		
		pool_sized(11, 11, 11, 5, 2);
		pool_sized(13, 11, 37, 5, 1);
		pool_sized(32, 32, 32, 5, 1);

		pool_sized(32, 32, 32, 8, 1);
		pool_sized(33, 31, 37, 8, 1);
	}

}  // namespace
#endif

