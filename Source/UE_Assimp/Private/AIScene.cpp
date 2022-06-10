// Fill out your copyright notice in the Description page of Project Settings.


#include "AIScene.h"

#include "AICamera.h"
#include "AILight.h"
#include "AIMesh.h"
#include "AINode.h"
#include "AIMaterial.h"
#include "assimp/mesh.h"
#include "AssimpMesh.h"
#include "ProceduralMeshComponent.h"
#include "assimp/cimport.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Serialization/BufferArchive.h"


UAIScene* UAIScene::InternalConstructNewScene(UObject* Parent, const aiScene* Scene)
{

	//todo check if object is already created and skip creation and return object 
	UAIScene* Object=	NewObject<UAIScene>(Parent,UAIScene::StaticClass(),NAME_None,RF_Transient);
	Object->scene= const_cast<aiScene*>(Scene);
	//Setup Meshes
	Object->OwnedMeshes.Reset();
	Object->OwnedMeshes.AddUninitialized(Scene->mNumMeshes);
	Object->OwnedLights.AddUninitialized(Scene->mNumLights);
	Object->OwnedCameras.AddUninitialized(Scene->mNumCameras);
	Object->OwnedMaterials.AddUninitialized(Scene->mNumMaterials);
	//Add Meshes
	if(Scene->HasMeshes())
	{
		for(unsigned Index=0;Index<Scene->mNumMeshes;Index++)
		{
			UAIMesh* NewMeshData=	NewObject<UAIMesh>(Object,UAIMesh::StaticClass(),NAME_None,RF_Transient);
			NewMeshData->Mesh=Scene->mMeshes[Index];
			Object->OwnedMeshes[Index]=NewMeshData;
		}
	}
	//Add Cams
	if(Scene->HasCameras())
	{
		for(unsigned Index=0;Index<Scene->mNumCameras;Index++)
		{
			UAICamera* Camera=	NewObject<UAICamera>(Object,UAICamera::StaticClass(),NAME_None,RF_Transient);
			Camera->camera=Scene->mCameras[Index];
			Object->OwnedCameras[Index]=Camera;
		}
	}
	//Add Lights
	if(Scene->HasLights())
	{
		for(unsigned Index=0;Index<Scene->mNumLights;Index++)
		{
			UAILight* Light=	NewObject<UAILight>(Object,UAILight::StaticClass(),NAME_None,RF_Transient);
			Light->Light=Scene->mLights[Index];
			
			Object->OwnedLights[Index]=Light;
		}
	}
	if(Scene->HasMaterials())
	{
		for(unsigned Index=0;Index<Scene->mNumMaterials;Index++)
		{
			UAIMaterial* Material=	NewObject<UAIMaterial>(Object,UAIMaterial::StaticClass(),NAME_None,RF_Transient);
			Material->Material=Scene->mMaterials[Index];
			Object->OwnedMaterials[Index]=Material;
		}
	}

	//RegisterNodes
	Object->RegisterNewNode(Scene->mRootNode);
	return Object;
}

TArray<UMeshComponent*> UAIScene::SpawnAllMeshes(FTransform Transform,TSubclassOf<AActor>ClassToSpawn)
{
	//TODO 
	TArray<UMeshComponent*> Meshes;
	if(scene)
	{

		for (auto Mesh :OwnedMeshes)
		{
			
			//mesh data will be owned by the scene object
			
			AActor* SpawnedActor=	GetWorld()->SpawnActor<AActor>(ClassToSpawn,Transform);
			UAssimpMesh* AssimpMesh=	NewObject<UAssimpMesh>(SpawnedActor,UAssimpMesh::StaticClass());
			AssimpMesh->RegisterComponent();
			AssimpMesh->SetWorldLocation(Transform.GetLocation()); 
			AssimpMesh->SetWorldRotation(Transform.GetRotation()); 
			AssimpMesh->AttachToComponent(SpawnedActor->GetRootComponent(),FAttachmentTransformRules::KeepWorldTransform);
			AssimpMesh->SetupMesh(Mesh);
			
		}
		
	}else
	{
		UE_LOG(LogTemp,Error,TEXT(" Assimp scene is not valid "));
		 
	}
	

return Meshes;
}

const TArray<UAIMesh*>&  UAIScene::GetAllMeshes() const
{

return OwnedMeshes;	
}

const TArray<UAIMaterial*>& UAIScene::GetAllMaterials() const
{
	return  OwnedMaterials;
}

void UAIScene::BeginDestroy()
{

	aiReleaseImport( scene);
	scene=nullptr;
	
	Super::BeginDestroy();
}


UAINode* UAIScene::GetRootNode()
{
	return OwnedRootNode;
}

UAIMesh* UAIScene::GetMeshAtIndex(int Index)
{
return  OwnedMeshes[Index];
}

const TArray<UAICamera*>& UAIScene::GetAllCameras() const
{
	return  OwnedCameras;
}

UTexture2D* UAIScene::GetEmbeddedTexture(FString FilePath,bool bIsNormalMap)
{
	auto EmbedTexture=scene->GetEmbeddedTexture(TCHAR_TO_UTF8(*FilePath));

	UTexture2D* Result=nullptr;
	
	
	if(!EmbedTexture)
	{
		UE_LOG(LogTemp,Error,TEXT("Texture was not found!"));
		return  nullptr;
	}

	const int TextureWidth=EmbedTexture->mWidth;
	const float TextureHeight=EmbedTexture->mHeight;
	const EPixelFormat PixelFormat =PF_B8G8R8A8;
	
	UE_LOG(LogTemp,Log,TEXT("Importing embedded texture X: %d  Y: %d"),TextureWidth,TextureHeight);
	if (EmbedTexture->mHeight != 0)
	{
		Result = UTexture2D::CreateTransient(EmbedTexture->mWidth, EmbedTexture->mHeight, PixelFormat);
	
	
		if (Result)
		{
			Result->bNotOfflineProcessed = true;
			uint8* MipData = static_cast<uint8*>(Result->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	
		
			//an array for pixels
			uint8* Pixels = new uint8[TextureWidth * TextureHeight * 4];
			for (int32 y = 0; y < TextureHeight; y++)
			{
				for (int32 x = 0; x < TextureWidth; x++)
				{
					//Get the current pixel
					int32 CurrentPixelIndex = ((y * TextureWidth) + x);
					//Get a random vector that will represent the RGB values for the current pixel
					const aiTexel CurrentPixel=	EmbedTexture->pcData[CurrentPixelIndex];
				
					Pixels[4 * CurrentPixelIndex] = CurrentPixel.b; //b
					Pixels[4 * CurrentPixelIndex + 1] = CurrentPixel.g; //g
					Pixels[4 * CurrentPixelIndex + 2] = CurrentPixel.b; //r
					Pixels[4 * CurrentPixelIndex + 3] = CurrentPixel.a; //set A channel always to maximum
				}
			}
			FMemory::Memcpy(MipData, Pixels, Result->PlatformData->Mips[0].BulkData.GetBulkDataSize());
			
		}
		
 
	
	
		
				
		Result->PlatformData->Mips[0].BulkData.Unlock();

		Result->UpdateResource();

		 
	}
	else
	{//Texture is compressed read it from memory
		const size_t size = EmbedTexture->mWidth;
		const auto BinaryData =reinterpret_cast<const unsigned char*>(EmbedTexture->pcData);
		const TArray<uint8> Buffer(BinaryData,size);
    			
    		
		Result= UKismetRenderingLibrary::ImportBufferAsTexture2D(GetWorld(),Buffer);
    			
    			
	}

	if(Result&&bIsNormalMap)
	{
		Result->CompressionSettings=TC_Normalmap;
		Result->SRGB=false;
		Result->UpdateResource();
	}
	return Result;
}


void UAIScene::RegisterNewNode(aiNode* InRootNode)
{
	
	//if its root node owned by scene

	UAINode* Object=	NewObject<UAINode>(this,UAINode::StaticClass(),NAME_None,RF_Transient);
	Object->Node= InRootNode;
	OwnedRootNode=Object;
	if(InRootNode->mNumChildren!=0)
	{
		//Follow Chain of nodes to init child nodes 
		for(unsigned Index=0;Index<InRootNode->mNumChildren;Index++)
		{
			OwnedRootNode->RegisterNewNode(InRootNode->mChildren[Index]);
		}
		
	}
		
}

EPixelFormat UAIScene::GetPixelFormat(const aiTexture* Texture)
{
	if(Texture->CheckFormat("rgba8888"))
	{
		return EPixelFormat::PF_R8G8B8A8;
	}else
	{
		UE_LOG(LogTemp,Fatal,TEXT("Pixel format not implemented"));
	}
	return  EPixelFormat::PF_Unknown;
}



