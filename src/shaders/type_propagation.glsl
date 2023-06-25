#version 460 core

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1 ) in;

//INCLUDE ir_types

//INCLUDE type_actions.h

//INCLUDE structs

//INCLUDE ir_tokens

layout(std430, binding = 3) readonly coherent buffer AstNodes {
	AstNode astNodes[];
};

layout(std430, binding = 0) readonly buffer IrCodegen {
    uint irCodegen[];
};

uint codegenPointer(uint nodeToken) {
	return irCodegen[nodeToken];
}

AstNode fetchAstNodeFromChildRef(int childRef) {
	return astNodes[childRef - 1];
}

uint getCodegenLength(uint nodeToken) {
	uint ptr = codegenPointer(nodeToken);
	if (ptr == 0) return 0;
	return irCodegen[ptr + 4];
}

uint getTypeActionPtr(uint nodeToken) {
    uint basePtr = codegenPointer(nodeToken);
    uint len = getCodegenLength(nodeToken);
    return basePtr + 5 + len;
}

int fetch_ref (uint ref, AstNode node, inout bool isLit) {
    if (ref > 4) return 0;
    int pos = node.children[ref].ref;

    // Traverse through empty nodes
    uint maxOut = 0;
    while (pos > 0 && maxOut < 100
        && codegenPointer(fetchAstNodeFromChildRef(pos).nodeToken) == 0) {

        pos = fetchAstNodeFromChildRef(pos).children[0].ref;

        if (fetchAstNodeFromChildRef(pos).nodeToken == LITERAL) {
            isLit = true;
            return pos;
        }

        maxOut++;
    }

    return pos;
}

void tryGetAction(inout bool done, inout IrType setsTo,
                  inout uint setting, uint pos) {

    if (done) return;

    AstNode node = astNodes[pos];

    uint ptr = getTypeActionPtr(node.nodeToken);

    uint len = irCodegen[ptr];

    if (len == 0) {
        done = true;
        return;
    }

    uint actionCount = (len - 2) / 2;

    for (uint i = 0; i < actionCount; i++) {
        uint setsToPtr = ptr + 3 + (i * 2);
        uint variant = irCodegen[setsToPtr];
        switch (variant) {
            case TYPE_ACTION_STATIC_TYPE:
                setsTo = IrType(irCodegen[setsToPtr + 1], 0, 0);
                break;
            case TYPE_ACTION_TYPE_OF_VREG:
                uint childIndex = irCodegen[setsToPtr + 1];
                bool isLit = false;
                int childRef = fetch_ref(childIndex, node, isLit);

                IrType childType = vregTypes[childRef];
                if (childType.base == 0) return;

                setsTo = favoured(setsTo, childType);

                break;
        }
    }

    uint settingVariant = irCodegen[ptr + 1];
    if (settingVariant == TYPE_ACTION_SET_SELF_VREG) {
        setting = pos + 1;
    } else if (settingVariant == TYPE_ACTION_SET_VREG) {
        uint childIndex = irCodegen[ptr + 2];
        setting = node.children[childIndex].ref - 1;
    }
}

void main() {
    uint pos = gl_GlobalInvocationID.x;
    bool done = false;
    IrType setsTo = IrType(0, 0, 0);
    uint setting = 0;
    for (int i = 0; i < 100; i++) {

        tryGetAction(done, setsTo, setting, pos);

        barrier();

        if (setting != 0) {
            done = true;
            vregTypes[setting] = setsTo;
            setting = 0;
        }
    }
}