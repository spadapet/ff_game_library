#include "pch.h"
#include "entity_domain.h"

ff::entity_base::entity_base(ff::entity_domain* domain)
    : domain_(domain)
{}

ff::entity_domain* ff::entity_base::domain() const
{
    return this->domain_;
}

ff::entity ff::entity_base::clone() const
{
    return this->domain_->clone(this);
}

bool ff::entity_base::active() const
{
    return this->domain_->active(this);
}

void ff::entity_base::active(bool value)
{
    this->domain_->active(this, value);
}

void ff::entity_base::destroy()
{
    this->domain_->destroy(this);
}

size_t ff::entity_base::hash() const
{
    return this->domain_->hash(this);
}

void ff::entity_base::event_notify(size_t event_id, void* event_args)
{
    this->domain_->event_notify(this, event_id, event_args);
}

ff::entity_event_sink& ff::entity_base::event_sink(ff::entity entity, size_t event_id)
{
    return this->domain_->event_sink(this, event_id);
}
