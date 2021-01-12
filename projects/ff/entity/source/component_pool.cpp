#include "pch.h"
#include "component_pool.h"

ff::component_pool_base::component_pool_base(std::type_index type, uint64_t bit)
    : type_(type)
    , bit_(bit)
{}

ff::component_pool_base::~component_pool_base()
{}

std::type_index ff::component_pool_base::type() const
{
    return this->type_;
}

uint64_t ff::component_pool_base::bit() const
{
    return this->bit_;
}

